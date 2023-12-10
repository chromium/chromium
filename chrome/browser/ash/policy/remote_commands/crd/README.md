Chrome Remote Desktop
---------------------


This directory contains the code that controls the remote admin triggered
Chrome Remote Desktop (CRD) sessions.

These sessions are started through a remote command (sent from the DPanel
devices page).

Remote Commands
---------------

There are 2 remote commands involved in starting CRD sessions:

  * FETCH_CRD_AVAILABILITY_INFO: This command queries the state
    of the device, and returns if CRD sessions are possible and if not, why.
  * START_CRD_SESSION: This command actually starts a CRD session.

Public APIs
-----------

The public API of this folder exists out of 3 classes:

  * `DeviceCommandFetchCrdAvailabilityInfoJob`: The `RemoteCommandJob` that handles the
    FETCH_CRD_AVAILABILITY_INFO remote command.
  * `DeviceCommandStartCrdSessionJob`: The `RemoteCommandJob` that handles the
    START_CRD_SESSION remote command.
  * `CrdAdminSessionController`: The long-lived controller that keeps track of
    the currently active CRD session.

