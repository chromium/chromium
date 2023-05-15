# WebView SDK Sandbox Test App and SDK

The WebView SDK Sandbox Test App and SDK are a standalone application and sdk
for testing WebView in a privacy sandbox environment. For more
information, please see [the
documentation](/android_webview/docs/privacy-sandbox.md).

This is *not* a production quality browser and does not implement suitable
security UI to be used for anything other than testing WebView. This should not
be shipped anywhere or used as the basis for implementing a browser.

To build a full-fledged browser for Android, we'd recommend building Chromium
for Android instead of using WebView:
https://www.chromium.org/developers/how-tos/android-build-instructions
