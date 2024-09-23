# WebView quick start

*** promo
Googlers may wish to consult http://go/clank-webview for Google-specific
developer guides.
***

[TOC]

## Overview

This is not a thorough guide for how to build WebView, but is the **fastest**
way to get a local build of WebView up and running.

### Building for preview Android releases

Googlers should see internal instructions. External contributors should switch
to a public (and finalized) Android release (there's no workaround).

## System requirements, tools, etc.

See general Android instructions for:

* [System
  requirements](/docs/android_build_instructions.md#System-requirements)
* [Installing
  `depot_tools`](/docs/android_build_instructions.md#Install-depot_tools)
* [Getting the code](/docs/android_build_instructions.md#Get-the-code) **or**
  [converting a Linux
  checkout](/docs/android_build_instructions.md#Converting-an-existing-Linux-checkout)
* [Installing build
  dependencies](/docs/android_build_instructions.md#Install-additional-build-dependencies)
  **and** [running hooks](/docs/android_build_instructions.md#Run-the-hooks)

## Install adb

If you don't already have `adb` installed, the fastest way is to add chromium's
Android SDK to your `$PATH`. If you use multiple terminal, you'll want to run
this command in each terminal:

```shell
$ source build/android/envsetup.sh
```

## Device setup

The recommend configuration is to use an **Android 10 (Q) emulator**. Android R
or higher is also OK. If you need to use Android N-P instead then you can use
the [old version of this guide](./quick-start-legacy.md). If you need to use any
other configuration, then you need to switch to the full [build
guide](./build-instructions.md) instead.

Set up an [Android emulator](/docs/android_emulator.md). You have 2 options for
this:

1. Preconfigured emulator image. Just run this command in your terminal, which
   will launch an emulator window when the emulator is ready. If anything goes
   wrong, see [the
   documentation](https://chromium.googlesource.com/chromium/src/+/HEAD/docs/android_emulator.md#Using-Prebuilt-CIPD-packages)
   or try the next option (see below).

  ```shell
  $ tools/android/avd/avd.py start \
      --avd-config tools/android/avd/proto/android_29_google_apis_x86.textpb --emulator-window
  ```

2. Android Studio Emulator image. [Install the Android Studio
   IDE](https://developer.android.com/studio/install) and then follow [these
   instructions](https://developer.android.com/studio/run/managing-avds)
   to launch the Device Manager GUI. Create an emulator with these settings:

   * Skin: any Pixel device skin is fine
   * Release name: **10**
   * ABI: **x86**
   * Target: **Google APIs**
   <!-- Keep this part in sync with /docs/android_emulator.md -->
   * Select "Show Advanced Settings" > scroll down:
      * Set internal storage to 4000MB
      * Set SD card to 1000MB
   * If in doubt, consult the
     [chromium
     documentation](https://chromium.googlesource.com/chromium/src/+/HEAD/docs/android_emulator.md#Using-Your-Own-Emulator-Image)
     to configure your emulator

   Once configured, click the **play** button to launch the emulator.

**Verify your emulator is ready:** after performing either of the steps above,
you should check your emulator by running:

```shell
# If everything worked correctly, this should say "device" in the right column.
$ adb devices
List of devices attached
emulator-5554   device
```

## Setting up the build

Configure GN args (run `gn args out/Default`) as follows:

```gn
target_os = "android"
target_cpu = "x86"

# Recommended: this lets you use System WebView Shell as a test app.
system_webview_shell_package_name = "org.chromium.my_webview_shell"
```

## Build, install, and switch WebView provider {#build}

```shell
# Build
$ autoninja -C out/Default system_webview_apk

# Install the APK
$ out/Default/bin/system_webview_apk install

# Tell Android platform to load a WebView implementation from this APK
$ out/Default/bin/system_webview_apk set-webview-provider
```

**That's it!** Your APK should be installed and should be providing the WebView
implementation for all apps on the system.

## Things to do next

### Start running an app

**Skip this section** if you already have an app you want to test.

You can start testing our your WebView APK with the System WebView Shell test
app. This also shows the WebView version at the top of the app, so you can
verify this is using the version you built locally. You can run this test app
like so:

```shell
# Build
$ autoninja -C out/Default system_webview_shell_apk

# Install
$ out/Default/bin/system_webview_shell_apk install

# Launch a URL
$ out/Default/bin/system_webview_shell_apk launch "https://www.google.com/"
```

For more info about WebView Shell, see [the docs](./webview-shell.md).

### Toggle features or commandline flags

**Skip this section** if you don't need to toggle a specific commandline flag.

If you exposed your flag in [ProductionSupportedFlagList.java], then you can
toggle the flag in WebView DevTools. For more info about WebView DevTools, see
[the docs](./developer-ui.md). You can launch WebView DevTools with:

```shell
$ adb shell am start -a "com.android.webview.SHOW_DEV_UI"
```

If the flag is not exposed, you can instead try following [these
steps](./commandline-flags.md).

### Debug with adb logcat

We recommend starting with "printf-style debugging" on Android:

1. Add some logs in your code:
   * In C++ code: add `LOG(ERROR) << "SOMETAG: <your log message goes here>";`
   * In Java code: add `org.chromium.base.Log.e("SOMETAG", "<your log message
     goes here>");`
2. Recompile and reinstall `system_webview_apk` (see
   [steps above](#build)). Re-launch your test app.
3. Read your log messages by running this command:
   `adb logcat | grep 'SOMETAG'`

For more guidance, refer to [the logging
documentation](/docs/android_logging.md).

### Run automated tests

We recommend starting with [integration
tests](./test-instructions.md#instrumentation-tests).

### Shutdown your emulator when you're done

The recommended way to turn off your emulator is to just close the emulator
window. If that doesn't work or you can't find the emulator window, then you can
safely shutdown your emulator by running `adb emu kill` in the terminal.

## Troubleshooting

If the install command succeeded but something else is wrong, the best way to
troubleshoot the problem is to query the state of the on-device
WebViewUpdateService:

```shell
$ adb shell dumpsys webviewupdate

Current WebView Update Service state
  Fallback logic enabled: true
  Current WebView package (name, version): (com.google.android.apps.chrome, 75.0.3741.0)
  Minimum WebView version code: 303012512
  Number of relros started: 1
  Number of relros finished: 1
  WebView package dirty: false
  Any WebView package installed: true
  Preferred WebView package (name, version): (com.google.android.apps.chrome, 75.0.3741.0)
  WebView packages:
    Valid package com.android.chrome (versionName: 58.0.3029.125, versionCode: 303012512, targetSdkVersion: 26) is  installed/enabled for all users
    Valid package com.google.android.webview (versionName: 58.0.3029.125, versionCode: 303012500, targetSdkVersion: 26) is NOT installed/enabled for all users
    Invalid package com.chrome.beta (versionName: 74.0.3729.23, versionCode: 372902311, targetSdkVersion: 28), reason: No WebView-library manifest flag
    Invalid package com.chrome.dev (versionName: 54.0.2840.98, versionCode: 284009811, targetSdkVersion: 24), reason: SDK version too low
    Invalid package com.chrome.canary (versionName: 75.0.3741.0, versionCode: 374100010, targetSdkVersion: 25), reason: SDK version too low
    Valid package com.google.android.apps.chrome (versionName: 75.0.3741.0, versionCode: 2, targetSdkVersion: 28) is  installed/enabled for all users
```

### Invalid package ... No WebView-library manifest flag

This APK does not contain a WebView implementation. Make sure you're building
`system_webview_apk`.

### Invalid package ... Version code too low

This shouldn't happen for userdebug builds. If it does, add this GN arg:

```gn
# Any number >= "Minimum WebView version code":
android_override_version_code = "987654321"
```

### Invalid package ... SDK version too low

The targetSdkVersion of your APK is too low (it must be >= the platform's API
level). This shouldn't happen for local builds using tip-of-tree chromium on
public OS versions (see [note](#Building-for-preview-Android-releases)).

*** note
**Note:** we only support local development using the latest revision of the
main branch. Checking out release branches introduces a lot of complexity, and
it might not even be possible to build WebView for your device.
***

### Invalid package ... Incorrect signature

This shouldn't happen for userdebug devices, and there's no workaround for user
devices. Make sure you have a userdebug device (you can check with `adb shell
getprop ro.build.type`).

### Valid package ... **is**  installed/enabled for all users

This is the correct state. If this is not the "preferred WebView package" or the
"current WebView package", call `set-webview-implementation` again.

### Valid package ... **is NOT** installed/enabled for all users

This shouldn't happen for `com.google.android.apps.chrome` (the recommended
package name). If you need to use a different package name, you may be able to
workaround this by enabling "redundant packages" (`adb shell cmd webviewupdate
enable-redundant-packages`), reinstalling, and running `set-webview-provider`
again.

Otherwise, please [reach out to the team][1].

### My package isn't in the list

Either your package didn't install (see below) or you chose a package name which
is [not eligible as a WebView provider](webview-providers.md#Package-name) for
this device. Double-check the package name in your GN args. If you're on AOSP
(any OS level), choose `"com.android.webview"`. If you're on L-M, choose
`"com.google.android.webview"`. In either case, you'll likely need to [remove
the preinstalled WebView
APK](/android_webview/tools/remove_preinstalled_webview.py).

### WebView shell doesn't show the correct version

Check the "Current WebView package" in the dumpsys output. You're probably
hitting one of the cases above.

### INSTALL\_FAILED\_UPDATE\_INCOMPATIBLE: Package ... signatures do not match previously installed version

Double check your emulator is Android 10 (Q) and that this is a **Google APIs**
image. Double check your GN args to make sure you are **not** setting the
`system_webview_package_name` argument (it's OK to set
`system_webview_shell_package_name`, but the other arg should be the default
value). If everything looks correct, try:

```shell
# Try uninstalling any WebView updates
$ adb uninstall com.android.webview

# If the uninstall command succeeded, then try installing your locally compiled
# WebView again:
$ out/Default/bin/system_webview_shell_apk install
$ out/Default/bin/system_webview_shell_apk set-webview-provider
```

### I couldn't install the APK/... is NOT installed.

This could fail for an even wider variety of reasons than already listed. Please
[reach out to the team][1].

### I couldn't **build** the APK

Try building Chromium. If that doesn't work, please reach out to [the chromium
team](https://groups.google.com/a/chromium.org/forum/#!forum/chromium-dev) for
general guidance. If `system_webview_apk` is the only troublesome target, please
reach out to the WebView team (see previous section).

### Apps using WebView crash with "java.lang.RuntimeException: Unable to start activity"

If apps using WebView crash with stack traces like the following:

```
AndroidRuntime: Shutting down VM
AndroidRuntime: FATAL EXCEPTION: main
AndroidRuntime: Process: org.chromium.webview_shell, PID: 6683
AndroidRuntime: java.lang.RuntimeException: Unable to start activity ComponentInfo{org.chromium.webview_shell/org.chromium.webview_shell.WebViewBrowserActivity}: android.util.AndroidRuntimeException: java.lang.reflect.InvocationTargetException
...
AndroidRuntime: Caused by: android.util.AndroidRuntimeException: java.lang.reflect.InvocationTargetException
...
AndroidRuntime: Caused by: org.chromium.base.library_loader.ProcessInitException: errorCode=2
...
AndroidRuntime: Caused by: java.lang.UnsatisfiedLinkError: dlopen failed: library "libc++_chrome.so" not found
...
```

This `UnsatisfiedLinkError` can occur when WebView is built using the
`target_cpu = "x86"` gn arg and the emulator architecture is x86_64. Double
check your emulator is Android 10 (Q) and uses the x86 ABI.

## What if I didn't follow these instructions exactly?

**Proceed at your own risk.** Building and installing WebView is, for a variety
of reasons, complex. If you've deviated from **any** of these instructions (and
don't know what you're doing) there's a good chance of making mistakes (some of
which don't have any error messages).

If you can't follow the quick start guide for some reason, please consult our
[general build instructions](build-instructions.md). You can also try the [old
version of the quick start](./quick-start-legacy.md).

[1]: https://groups.google.com/a/chromium.org/forum/#!forum/android-webview-dev
[ProductionSupportedFlagList.java]: https://source.chromium.org/chromium/chromium/src/+/main:android_webview/java/src/org/chromium/android_webview/common/ProductionSupportedFlagList.java
