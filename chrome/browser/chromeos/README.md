chrome/browser/chromeos
=======================

This directory should contain non UI Chrome OS specific code that has
`src/chrome` dependencies.

Code here should not contain any `ash/` dependencies or `chrome/browser/ui`
dependencies. Any such UI code should be moved to
[`chrome/browser/ui/ash`](/chrome/browser/ui/ash/README.md)
(which may depend on code in this directory).

Example:

* The Chrome OS network portal detection model lives in
  `chrome/browser/chromeos/net/network_portal_detector_impl.cc`.

* The notification controller for network portal detection lives in:
  `chrome/browser/ui/ash/network/network_portal_notification_controller.cc`
  (which depends on *chrome/browser/ui*, and
  *chrome/browser/chromeos/net/network_portal_detector_impl.h*.
