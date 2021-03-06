#include "simdata.h"
#include "owneddata.h"
#include <XPLMDataAccess.h>
#include <XPLMUtilities.h>
#include "xpapiface_msg.h"
#include "navcalc.h"
#include "apxplane9standard.h"
#include <math.h>

extern bool m_readfp;

APXPlane9Standard::APXPlane9Standard(std::ostream& logfile):
        LogicHandler(logfile),
        m_simAPState(0),
        m_simFDMode(0),
        m_simVVI(0),
        m_simHDG(0),
        m_simAPSPDisMach(0),
        m_simBarAlt(0),
        m_simAltWindow(0),
        m_internalAPState(0),
        m_internalVVI(0),
        m_internalHDG(0),
        m_logfile(logfile)
{}

APXPlane9Standard::~APXPlane9Standard()
{
    delete m_simAPState;
    delete m_simFDMode;
    delete m_simAltWindow;
    delete m_simVVI;
    delete m_simBarAlt;
    delete m_simAPSPDisMach;
    delete m_simHDG;

    delete m_internalAPState;
    delete m_internalVVI;
    delete m_internalHDG;
}

bool APXPlane9Standard::registerDataRefs()
{
    m_simAPState = new SimData<int>("sim/cockpit/autopilot/autopilot_state","Autopilot bitfield", RWType::ReadWrite);
    m_simFDMode = new SimData<int>("sim/cockpit/autopilot/autopilot_mode", "FD : Off, On, Auto", RWType::ReadOnly);
    m_simAltWindow = new SimData<float>("sim/cockpit/autopilot/altitude","Autopilot Alt window", RWType::ReadOnly);
    m_simVVI = new SimData<float>("sim/cockpit/autopilot/vertical_velocity","Autopilot VVI",RWType::ReadWrite);
    m_simBarAlt = new SimData<float>("sim/flightmodel/misc/h_ind","indicated barometric alt", RWType::ReadOnly);
    m_simAPSPDisMach = new SimData<bool>("sim/cockpit/autopilot/airspeed_is_mach","AP Spd shows mach",RWType::ReadWrite);
    m_simHDG = new SimData<float>("sim/cockpit/autopilot/heading_mag","XP AP HDG",RWType::ReadWrite);
    return true;
}

bool APXPlane9Standard::initState()
{
    m_internalAPState->set(1);
    m_internalHDG->set(m_simHDG->data());
    m_internalVVI->set(m_simVVI->data());
    return true;
}

bool APXPlane9Standard::processState()
{

    m_simAltWindow->poll();
    m_simBarAlt->poll();
    m_simAPSPDisMach->poll();
    m_simVVI->poll();
    m_simHDG->poll();

    float heading = m_simHDG->data();
    float last_received_heading = m_internalHDG->data();

    bool ap_spd_is_mach = m_simAPSPDisMach->data();
    float vs_request = m_internalVVI->data();

    string result;
    struct sockaddr_in fromaddr;
    fromaddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if ( fabs(last_received_heading-heading) > 1 && fabs(last_received_heading - heading) < 359)
    {
        double ap_hdg_diff = -Navcalc::getSignedHeadingDiff(last_received_heading, heading);
        if (ap_hdg_diff > 20.0)
            heading = Navcalc::trimHeading(heading + 10.0);
        else if (ap_hdg_diff < -20.0)
            heading = Navcalc::trimHeading(heading - 10.0);
        else
            heading = last_received_heading;
        m_simHDG->set(heading);
    } else
    {
        m_internalHDG->set(Navcalc::round(heading));
    }

    if(m_readfp) {
        m_simAPState->poll();
        m_simFDMode->poll();
        int xp_fd_mode = m_simFDMode->data();
        int xp_ap_state = m_simAPState->data();
        m_simAPState->set(0);
        m_simFDMode->set(0);
        result = infoserver.getFlightPlan(fromaddr);
        if(result.length() != 0) {
          infoserver.processFlightPlan(result);
        } else {
           m_logfile << "WARNING: Got empty response, maybe we cannot connect to infoserver"  << std::endl;
        }
        m_simAPState->set(xp_ap_state);
        m_simFDMode->set(xp_fd_mode);
        m_readfp = false;
    }

    return true;
}

bool APXPlane9Standard::processInput(long input)
{

    return true;
}

bool APXPlane9Standard::publishData()
{
    m_internalAPState = new OwnedData<int>("plugins/org/vasproject/xpfmcconn/autopilot/autopilot_state");
    m_internalVVI = new OwnedData<float>("plugins/org/vasproject/xpfmcconn/autopilot/vertical_velocity");
    m_internalHDG = new OwnedData<float>("plugins/org/vasproject/xpfmcconn/autopilot/heading");

    m_internalHDG->set(m_simHDG->data());
    m_internalVVI->set(m_simVVI->data());
    m_internalAPState->set(0);

    m_internalAPState->registerRead();
    m_internalVVI->registerReadWrite();
    m_internalHDG->registerReadWrite();
    return (m_internalAPState->isRegistered() && m_internalVVI->isRegistered() && m_internalHDG->isRegistered());
}

bool APXPlane9Standard::unpublishData()
{
    m_internalAPState->unregister();
    m_internalVVI->unregister();
    m_internalHDG->unregister();

    delete m_internalAPState;
    m_internalAPState = 0;
    delete m_internalVVI;
    m_internalVVI = 0;
    delete m_internalHDG;
    m_internalHDG = 0;

    return true;
}

float APXPlane9Standard::processingFrequency()
{
    return -7;
}


