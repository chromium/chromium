# //android_webview/support_library/

This folder contains the boundary interface adapters for the AndroidX WebKit
Support Library
([`androidx.webkit.*`](https://developer.android.com/reference/androidx/webkit/package-summary)).

## Architecture & Startup Flow

- **`SupportLibWebViewChromiumFactory`**: Entry point implementing
  `WebViewProviderFactoryBoundaryInterface`. Adapts AndroidX WebKit boundary
  interfaces to `StartupController` (`startUpWebView`), `ProfileStore`,
  `AwTracingController`, `AwServiceWorkerController`, and shared glue adapters
  via `WebViewChromiumAwInit` and `WebkitToSharedGlueConverter`.

## Folder Dependencies

- `//android_webview/java/` must **never** depend on this directory.

## See Also

- [//android_webview/glue/](/android_webview/glue/README.md)
- [//android_webview/java/](/android_webview/java/README.md)
