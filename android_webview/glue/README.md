# //android_webview/glue/

This folder contains the WebKit framework glue layer that adapts public Android
framework APIs
([`android.webkit.*`](https://developer.android.com/reference/android/webkit/package-summary))
to WebView's Chromium implementation.

## Architecture & Startup Flow

- **`WebViewChromiumFactoryProvider`**: Framework-facing singleton instantiated
  by the OS during WebView provider initialization. Owns framework-level
  adapters (`CookieManagerAdapter`, `WebIconDatabaseAdapter`,
  `WebViewDatabaseAdapter`, `StaticsAdapter`) and initializes
  `StartupController`.
- **`StartupDelegateImpl`**: Implements `StartupController.Delegate` to supply
  framework-dependent hooks (resource bundle setup via `R.onResourcesLoaded`,
  hardware/software draw function tables, child process launcher configuration,
  selection action menu client, and post-startup default profile initialization)
  without coupling `//android_webview/java/` to framework SDK internals.
- **`WebViewChromiumAwInit`**: Holds glue-shared `ProfileStore` and default
  profile initialization (`DefaultProfileHolder`), along with shared accessors
  bridged to `//android_webview/support_library/`.
- **`WebkitToSharedGlueConverter`**: Exposes shared glue adapters and
  `WebViewChromiumAwInit` bridges to `//android_webview/support_library/`.

## Folder Dependencies

- `//android_webview/java/` must **never** depend on this directory.
- `//android_webview/glue/` depends on `//android_webview/java/` and Android
  framework SDK types (`android.webkit.*`, `android.view.*`).

## See Also

- [//android_webview/support_library/](/android_webview/support_library/README.md)
- [//android_webview/java/](/android_webview/java/README.md)
