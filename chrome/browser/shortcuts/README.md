# Shortcuts

This folder implements the logic behind the `Create Shortcut` flow that is seen
in the three dot menu in Chrome. This folder contains:

* Core business logic behind downloading metadata from a site required to
create a shortcut and icon manipulation techniques to badge icons.
* Platform-specific implementations to integrate these shortcuts in the
respective operating systems it is triggered on as well as ensuring that
they work on Chrome. This is not used on ChromeOS.