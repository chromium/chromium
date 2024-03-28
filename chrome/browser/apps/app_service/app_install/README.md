# App Install Service

This directory contains the App Install Service, a ChromeOS only component of
the App Service that unifies installation of different app types through a
common interface.

There are two entry points for the interface:
- [AppInstallService](app_install_service.h) for C++ clients.
- almanac://install-app for web clients, see
  [AppInstallNavigationThrottle](app_install_navigation_throttle.h) for further
  details.

This directory is currently in development and doesn't have full functionality
yet. As of 2024/03 it only supports the installation of web apps with metadata
hosted in the almanac database (go/cad-melting-pot-prd).
