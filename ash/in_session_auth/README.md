# In-Session User Authentication Dialog

This Ash dialog is for authenticating the user during a user session. The
primary use case is WebAuthn, where a platform daemon (u2fd) receives an
authentication request from the Chrome browser, and needs to initiate a user
authentication dialog which could involve fingerprint, PIN and password. More
authentication methods, such as SmartLock and smart cards, might be added in
the future.

This dialog is designed to be reused by other projects that need to trigger
in-session user authentication from ChromeOS, such as authenticating for ARC
apps.

This dialog is controlled by ash::AuthDialogController. When the user provides
a credential, the controller talks to cryptohome via
ash::AuthPerformer for authentication.
