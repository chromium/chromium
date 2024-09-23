# Entities used on the login/lock screen

The main entities used to show the login/lock screen UI.

- `//ash/public/cpp/`:
  - This folder contains inferfaces that are implemented in ash or chrome and
are used to communicate between ash and chrome services.
  - [`LoginScreenClient`](/ash/public/cpp/login_screen_client.h) - handles
method calls sent from ash to chrome & handles messages from chrome to ash.
Forwards some of the calls to the `Delegate`.

- `//chrome/browser/ui/ash/login/`:
  - This folder contains implementations of login and OOBE UIs.
  - [`LoginDisplayHostMojo`](/chrome/browser/ui/ash/login/
login_display_host_mojo.h) - a `LoginDisplayHost` instance that implements
`LoginScreenClient` and sends requests to the views-based sign in. Handles calls
like `HandleAuthenticateUserWith...()`. Owned by
`ChromeBrowserMainExtraPartsAsh`.

- `//ash/login/`:
  - This folder contains the implementation of login UI views (buttons, inputs,
etc), and additional classes that handle notifications and update the UI. Also
see [ash/login/ui/README.md](/ash/login/ui/README.md)
  - [`LoginScreenController`](/ash/login/login_screen_controller.h) - mostly
forwards requests to `LoginScreenClient` or calls `Shelf` APIs directly. Owned
by `Shell`.
  - [`LoginDataDispatcher`](/ash/login/ui/login_data_dispatcher.h) - provides
access to data notification events needed by the lock/login screen (via the
observer). Owned by `LoginScreenController`.
  - [`LockContentsView`](/ash/login/ui/lock_contents_view.h) - hosts the root
view for the login/lock screen. Receives notifications from the
`LoginDataDispatcher` and updates the UI. Owned by `LockScreen`.

- `//chrome/browser/ash/login/lock/`:
  - This folder contains the lock screen - specific logic for the login UIs.
  - [`ViewsScreenLocker`](/chrome/browser/ash/login/lock/views_screen_locker.h)
handles calls between ash and chrome on the lock screen by implementing
Delegate interfaces.
