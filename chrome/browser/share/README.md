## //chrome/browser/share

This directory contains the implementation of some of the sharing, social, and
community features in Chromium. Specifically, this directory contains the
implementations for parts of:

* QR code generation
* Link-to-text / link-to-highlight
* Screenshot editing
* The Android Chromium share hub

Related directories:

* [//chrome/browser/sharing](../sharing): contains a different subset of
  sharing-related functionality for historical reasons. Eventually these
  directories should be merged
* [//chrome/android/java/src/org/chromium/chrome/browser/share](../../android/java/src/org/chromium/chrome/browser/share):
  pre-modularization share code, or share code that is too entwined with
  ChromeActivity to easily move
* [//components/send_tab_to_self](../../../components/send_tab_to_self):
  Send-tab-to-self implementation
* [//components/qr_code_generator](../../../components/qr_code_generator):
  QR code generator backend (a Mojo service)
* [//chrome/browser/ui/views](../ui/views): desktop implementations of various
  sharing UIs
