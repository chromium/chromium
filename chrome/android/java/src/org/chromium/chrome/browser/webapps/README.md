# Android Webapps & WebAPKs (`chrome/android/java/.../webapps`)

This directory contains the primary Java implementations for Web Applications
and WebAPKs in Chrome on Android.

Following Chromium's
[documentation guidelines](/docs/documentation_guidelines.md), this document
highlights **lifecycle coordination, class interactions, and subsystem wiring**.
For individual class/method APIs, refer to the Java source files.

For full sequence diagrams, class graphs, and deep dives, consult the canonical
documentation:

- **[Android Web Apps Architecture (WebAPKs and TWAs)](/components/webapps/docs/android_architecture.md)**
- **[Registration and Permission Delegation](/components/webapps/docs/android_registration_and_permissions.md)**
- **[Android Testing Guide](/components/webapps/docs/android_testing_guide.md)**
- **[TWA Launch Parameters Handling](/components/webapps/docs/android_twa_launch_params.md)**

## Lifecycle & Subsystem Wiring

### 1. Activity & Window Structure

```
BaseCustomTabActivity
         │ (inherits)
         ▼
  WebappActivity ──► Hosts WebappActivityCoordinator & WebApkActivityCoordinator
         │
         ├──► Warms up storage via WebappDataStorage & WebappRegistry
         ├──► Triggers usage sync via WebApkSyncService (JNI)
         ├──► Dispatches permission delegation via InstalledWebappRegistrar
         └──► Checks for manifest updates via WebApkUpdateManager
```

- **Custom Tab Foundation:** `WebappActivity` extends `BaseCustomTabActivity`,
  reusing Chromium's Custom Tab rendering and session engine while hiding
  standard browser controls (omnibox, tab switcher) to deliver a standalone app
  experience.
- **Deferred Startup Coordinator:** On launch, `BaseCustomTabActivity` delegates
  to `WebappActivityCoordinator` and `WebApkActivityCoordinator` to run
  non-blocking startup tasks: initializing storage (`WebappRegistry`), notifying
  Chrome Sync via JNI (`WebApkSyncService`), delegating permissions on Android
  13+ (`PermissionUpdater`), and initiating manifest update checks.

### 2. Manifest Update Pipeline

```
WebApkUpdateManager (compares live manifest vs WebappInfo)
         │ (update needed)
         ▼
WebApkUpdateManagerJni (serializes update request to file)
         │
         ▼
BackgroundTaskScheduler ──► Schedules WebApkUpdateTask
                                     │ (executes when charging, unmetered network, & app is closed)
                                     ▼
Native WebApkInstaller ──► Sends update request to WebAPK minting server
```

- When `WebApkUpdateManager` detects significant changes to the site's manifest
  (name, icons, colors, or scope), it serializes the update request to disk via
  JNI and schedules a background job (`WebApkUpdateTask`) through Android's
  `BackgroundTaskScheduler`.
- **Foreground Invariant:** The update task checks whether the WebAPK activity
  is running before executing; if the app is active in the foreground, the
  update is deferred to prevent runtime state corruption.
