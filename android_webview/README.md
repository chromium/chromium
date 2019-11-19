# Android WebView

Android WebView is an Android system component for displaying web content.
[WebView](https://developer.android.com/reference/android/webkit/WebView) (and
the [related Android classes][1]) are implemented by the code in the
`//android_webview/` folder.

Android WebView is a content embedder, meaning it depends on code in
`//content/` and lower layers (ex. `//net/`, `//base/`), but does not depend on
sibling layers such as `//chrome/`.

This directory contains the Android WebView implementation, as well as the
implementation for the [AndroidX Webkit support library][2].

## Want to use WebView in an Android app?

Please consult our API documentation and app development guides:

* [Android Frameworks classes][1]
* [AndroidX Webkit support library][2]
* [Development guides](https://developer.android.com/guide/webapps)

## Want to build and install WebView on a device?

See our [Chromium developer documentation](docs/README.md).

[1]: https://developer.android.com/reference/android/webkit/package-summary
[2]: https://developer.android.com/reference/androidx/webkit/package-summary
