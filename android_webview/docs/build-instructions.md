# WebView Build Instructions

*** promo
Building WebView for the first time? Please see the [quick
start](quick-start.md) guide first.
***

[TOC]

## Overview

This is meant to be a comprehensive guide for building WebView, within the
limits of what is possible in a **public** chromium checkout. While this is
sufficient for most cases, Googlers may wish to consult [internal
instructions][1] to get a checkout including closed-source code, which is
necessary if:

* You work on features depending on this closed-source code
* You want to use the "downstream" targets (ex. `system_webview_google_apk`),
  **or**
* You need to install on a preview Android release

## System requirements, tools, etc.

See general Android instructions for:

* [System
  requirements](/docs/android_build_instructions.md#System-requirements)
* [Installing `depot_tools`](/docs/android_build_instructions.md#Install-depot_tools)
* [Getting the code](/docs/android_build_instructions.md#Get-the-code) **or**
  [converting a Linux
  checkout](/docs/android_build_instructions.md#Converting-an-existing-Linux-checkout)
* [Installing build
  dependencies](/docs/android_build_instructions.md#Install-additional-build-dependencies)
  **and** [running hooks](/docs/android_build_instructions.md#Run-the-hooks)

## Device setup

For the minimum requirements, please see [Device Setup](device-setup.md).

## Setting up the build

Configure GN args (run `gn args out/Default`) as follows:

```gn
target_os = "android"

# See "Figuring out target_cpu" below
target_cpu = "arm64"

# Not always necessary, see "Changing package name" below
system_webview_package_name = "..."

# Optional: speeds up build time. For instructions, refer to
# https://chromium.googlesource.com/chromium/src/+/main/docs/linux/build_instructions.md#use-reclient
use_remoteexec = true
```

### Figuring out target\_cpu

Please see the [Chromium
instructions](/docs/android_build_instructions.md#Figuring-out-target_cpu).

## Building WebView

You can build WebView with:

```shell
autoninja -C out/Default system_webview_apk
```

### Changing package name

Unlike most Android apps, WebView is part of the Android framework. One of the
consequences of this is that the WebView implementation on the device can only
be provided by a predetermined set of package names (see
[details](webview-providers.md#Package-name)). Depending on the chosen build
target, you may need to change the package name to match one of the following:

<!-- Keep this table in sync with webview-providers.md -->
| API level            | Has GMS vs. AOSP? | Allowed package names |
| -------------------- | ----------------- | --------------------- |
| >= Q                 | AOSP    | `com.android.webview` **(default, preinstalled)** |
| >= Q                 | Has GMS | `com.google.android.webview` **(default, preinstalled)**<br>`com.google.android.webview.beta`<br>`com.google.android.webview.dev`<br>`com.google.android.webview.canary`<br>`com.google.android.webview.debug` **(only userdebug/eng)**<br>`com.android.webview` **(only userdebug/eng)** |

`system_webview_apk` uses `com.android.webview` as the package name by
default. If your device allows this package name, continue to the [next
section](#removing-preinstalled-webview). Otherwise, you can change the package
name by setting the `system_webview_package_name` GN arg (ex.
`system_webview_package_name = "com.google.android.webview"`).

See [internal instructions][1] for the Google-internal build targets
(`system_webview_google_apk`).

### Removing preinstalled WebView

If WebView is preinstalled (under the chosen package name) in the device's
system image, you'll also need to remove the preinstalled APK (otherwise, you'll
see signature mismatches when installing). **You can skip this step** if
You [chose a package name](#Changing-package-name) which is not marked as
"(preinstalled)."

Otherwise, you can remove the preinstalled WebView like so:

```shell
android_webview/tools/remove_preinstalled_webview.py
```

*** note
If you're using an emulator, make sure to [start it with
`-writable-system`](/docs/android_emulator.md#writable-system-partition)
**before** removing the preinstalled WebView.
***

If the script doesn't work, see the [manual steps](removing-system-apps.md).

## Installing WebView and switching provider

For help connecting your Android device, see the [Chromium
instructions](/docs/android_build_instructions.md#Installing-and-Running-Chromium-on-a-device).

You can install a locally compiled APK like so:

```shell
# Install the APK
out/Default/bin/system_webview_apk install

# Tell Android platform to load a WebView implementation from this APK
out/Default/bin/system_webview_apk set-webview-provider
```

## Start running an app

See [Start running an app](quick-start.md#start-running-an-app) from the quick
start.

## Troubleshooting

Please see the [Troubleshooting](quick-start.md#troubleshooting) section in the
quick start.

[1]: http://go/clank-webview/build_instructions.md
[2]: https://groups.google.com/a/chromium.org/forum/#!forum/android-webview-dev
