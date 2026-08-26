# Android WebAPK Browser Native Code (`chrome/browser/android/webapk`)

This directory contains the C++ browser-process components for WebAPKs in
Chromium on Android.

Following Chromium's
[documentation guidelines](/docs/documentation_guidelines.md), this document
describes **how native WebAPK components bridge Java UI/coordinators, the WebAPK
minting server, and Chrome Sync**. For class-specific API details, see the
header files.

For the comprehensive architectural guide covering the complete Java/C++ call
graphs, see
[Android Web Apps Architecture](/components/webapps/docs/android_architecture.md).

## Subsystems & JNI Bridge Architecture

```
Java WebApkActivityCoordinator / WebApkUpdateManager
                     │
                     ▼ (JNI)
[Native WebAPK Services] chrome/browser/android/webapk/
      ├── WebApkInstaller ──► WebAPK Minting Server (Requests signed APK)
      ├── WebApkUpdateManager ──► Manifest Serialization & Update Dispatch
      └── WebApkSyncBridge / WebApkDatabase ──► Chrome Sync Engine
```

### 1. Installation Pipeline

- **Tri-Party Minting Protocol:** [`WebApkInstaller`](webapk_installer.h)
  receives installation requests from
  [`AppBannerManagerAndroid`](/components/webapps/browser/android/app_banner_manager_android.h)
  or the user menu. It packages the parsed Web App Manifest and icon hashes,
  sends an asynchronous request to the WebAPK minting server, and receives an
  installation token. It passes this token across JNI to Java
  (`WebApkInstaller.java`), which delegates the APK download and installation to
  Google Play via `GooglePlayWebApkInstallDelegate`.
- **Install Management:** [`WebApkInstallService`](webapk_install_service.h) is
  the profile-keyed service that queues, deduplicates, and manages active
  installation requests across tabs.

### 2. Synchronization & Persistence

- **[`WebApkSyncBridge`](webapk_sync_bridge.h)** &
  **[`WebApkDatabase`](webapk_database.h)**: Synchronizes installed WebAPK
  specifics with Chrome Sync and persists local state in LevelDB, enabling
  WebAPK restoration on new Android devices via
  [`WebApkRestoreManager`](webapk_restore_manager.h).

### 3. Update Dispatch & Invariants

- **Background Update Bridge:** Free JNI functions in
  [`webapk_update_manager.cc`](webapk_update_manager.cc) serialize updated
  manifest and icon data to disk when Java detects changes. When the scheduled
  background task executes, `WebApkInstaller` reads the file, requests an
  updated token from the minting server, and triggers the update via Google
  Play.
- **Foreground Suppression Invariant:** Background update tasks defer execution
  while the target WebAPK is currently open in the foreground to prevent runtime
  state corruption.

## Related Directories

- Java WebAPK and Webapp logic:
  [`chrome/android/java/src/org/chromium/chrome/browser/webapps/`](/chrome/android/java/src/org/chromium/chrome/browser/webapps/)
- WebAPK Shell and Client Libraries:
  [`chrome/android/webapk/`](/chrome/android/webapk/README.md)
- Shared Web Apps Component:
  [`components/webapps/browser/android/`](/components/webapps/browser/android/)
