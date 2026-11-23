# //android_webview/java/

This folder contains most of WebView's Java code that runs within the browser
process of an embedding app.

## Startup Architecture

Chromium initialization in WebView follows a 3-tier hierarchy:

1. **Orchestration (`StartupController`)**:
   Process-wide singleton managing the startup state machine
   (`INIT_NOT_STARTED`, `INIT_POSTED`, `INIT_FINISHED`), UI thread looper
   binding, post-startup run queues (`addTaskToRunQueue`,
   `addStartupFinishedCallback`), synchronous and asynchronous startup triggers
   (`startUpWebView`), and glue hooks via `StartupController.Delegate`.
2. **Scheduling Units (`StartupTasks` & `StartupTasksRunner`)**:
   Stateless top-level scheduling tasks queued into `StartupTasksRunner` across
   pre-browser-process, background non-UI-thread, and post-browser-process
   stages.
3. **Domain Component Initialization**:
   Feature-specific startup logic lives in its owning domain class (e.g.,
   `VariationsSeedLoader`, `AwSupervisedUserUrlClassifier`,
   `AwMetricsLogUploader`, `AwAccessibilityStateVisibilityManager`,
   `AwMinidumpUploader`) rather than in `StartupTasks` or `AwBrowserProcess`.

Supporting startup components:

- **Compatibility (`CompatQuirks`)**: Centralizes `targetSdkVersion` and
  `CompatChanges` behavioral quirks.
- **Test & Shell Startup (`TestStartupUtils`)**: Instrumentation tests
  (`AwActivityTestRule`) and `AwShellActivity` (`//android_webview/test/shell/`)
  initialize WebView through `StartupController` using
  `TestStartupUtils.startForTesting()` and `TestStartupDelegate`, exercising
  the production startup state machine and task queues without depending on
  `//android_webview/glue/`.

## Folder Dependencies

- `//android_webview/java/` must **never** depend on any other WebView Java
  layer (such as `//android_webview/glue/` or
  `//android_webview/support_library/`).

## See Also

- [//android_webview/glue/](/android_webview/glue/README.md)
- [//android_webview/support_library/](/android_webview/support_library/README.md)
