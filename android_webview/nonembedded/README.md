# //android\_webview/nonembedded/

This folder holds WebView code that is run under WebView's own UID and _not_
within an embedding app, such as global services and developer UI. Because these
processes aren't associated with any particular WebView-embedding app,
`ContextUtils.getApplicationContext()` will return a Context associated with the
WebView provider package itself.

These processes:

- Support commandline flags on debuggable devices
- Do not support UMA or Finch (we only maintain these for the embedded use case)
- Do not support talking to the Network Service (that runs in the browser
  process) or renderer services (those run in the context of the embedding app)
- Do not support using WebView instances in their UI
- Are not associated with any particular WebView-embedding app on the system
- May freely access the WebView provider's data directory, cache directory, etc.
  (`ContextUtils.getApplicationContext()` will return a Context associated with
  the WebView provider package)

## UI process

The `:webview_apk` process is typically for user-facing content (ex.
Activities). This is the process where developer UI code runs, but it's also the
process where the LicenseContentProvider runs.

This process initializes the native library, as the LicenseContentProvider loads
license information over JNI/C++.

## Service process

The `:webview_service` process runs in the background and contains
non-user-facing components (ex. Services). This is notably used by
variations (Finch) to fetch seeds over the network, crash uploading, and
Developer UI (to transfer information between the UI and embedded WebViews).

This process does **not** load the native library (Java-only, no JNI/C++), as we
aim to keep this process as light as possible to minimize the impact to the
system.

**Note:** this process may be long-lived. DeveloperUiService may run as a
"foreground service," in which case the system will prioritize this process over
most others when the system is low on memory.

## Monochrome compatibility

Because Monochrome declares both Chrome's and WebView's
[components](https://developer.android.com/guide/components/fundamentals#Components),
we need to take several precautions to make sure these don't interfere with each
other:

- Activities need to be marked with `android:process=":webview_apk"` to ensure
  process isolation from Chrome's browser process.
- Services need to be marked with `android:process=":webview_service"` to ensure
  the service runs in a separate process from both the UI code and Chrome's
  code.
- ContentProviders also need an explicit `android:process`, although this may
  declare either process depending on what it needs.
- Activities also need an explicit `android:taskAffinity`, otherwise using the
  WebView Activity will replace the current Chrome session from the task stack
  (or vice versa). The taskAffinity should include the WebView package name,
  otherwise Activities from different WebView channels will trample each other.
