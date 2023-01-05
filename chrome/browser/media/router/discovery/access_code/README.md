# Overview
Access Code Casting is an extension of the [Media Router](http://www.chromium.org/developers/design-documents/media-router) that allows for casting via an access code.

# External Uses
The external product that currently only uses this feature is [Cast Moderator](g.co/castmoderator/setup)

# User Flow
The code within this directory handles the back end of an access code within
Chrome.
1) An access code is submitted
2) Check with the server if this is a valid access code
3) Construct a device with returned info from server
4) Attempt to add this device to the media router
5) Attempt to start a casting session to this device
6) (Optional) Store this device in prefs

# Important Classes
*access_code_cast_sink_service*
The communication from the frontend to backend is handled by this class. This
class also handles the lifetimes of other objects that are constructed within
this directory.

This class also handles stored device logic on startup/whenever a route is
removed.

*access_code_cast_discovery_interface*
Handles communication between the server and Chrome

*access_code_cast_pref_updater*
Handles storage of prefs within Chrome.

*access_code_cast_service_factory*
Handles the construction of the AccessCodeCastSinkService and ensures lifetime
is valid within the constrains of the Media Router lifetime.
