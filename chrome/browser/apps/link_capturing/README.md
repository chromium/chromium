# Link Capturing

This folder implements the "link capturing" browser feature, where clicking a
link in a browser tab may open an installed app. This folder contains:

* The core business logic for determining what link clicks are eligible to be
  captured.
* Platform-specific delegates for connecting this business logic to app
  platforms, allowing apps to be found and launched
* The backend logic for various Intent Picker UIs (Intent Chip, Intent Picker,
  Infobar), which provide user control over link capturing behavior in the
  browser.
