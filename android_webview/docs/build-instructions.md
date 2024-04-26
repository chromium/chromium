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

[Similarly to
Chrome](/docs/android_build_instructions.md#Multiple-Chrome-APK-Targets),
WebView can be compiled with a variety of build targets.

_TODO(crbug.com/41454956): document the differences between each target._

First, you should figure out your device's integer API level, which determines
which build targets will be compatible with the version of the OS on your
device:

```shell
adb shell getprop ro.build.version.sdk
```

*** promo
**Tip:** you can convert the API level integer to the release's dessert
codename with [this
table](https://developer.android.com/guide/topics/manifest/uses-sdk-element.html#ApiLevels).
This developer guide uses API integers and release letters interchangeably.
***

Then you can build one of the following targets:

```shell
# For L+ (21+) devices (if on N-P, see "Important Notes for N-P")
autoninja -C out/Default system_webview_apk

# For N-P (24-28) devices (not including TV/car devices)
autoninja -C out/Default monochrome_public_apk

# For Q+ (29+) devices
autoninja -C out/Default trichrome_webview_apk
```

<!--
  TODO(crbug.com/41454956): merge this and the other "Tip" when we
  document the Trichrome target in detail.
-->
*** promo
**Tip:** building `trichrome_webview_apk` will automatically build its
dependencies (i.e., `trichrome_library_apk`).
***

### Changing package name

Unlike most Android apps, WebView is part of the Android framework. One of the
consequences of this is that the WebView implementation on the device can only
be provided by a predetermined set of package names (see
[details](webview-providers.md#Package-name)). Depending on the chosen build
target, you may need to change the package name to match one of the following:

<!-- Keep this table in sync with webview-providers.md -->
| API level            | Has GMS vs. AOSP? | Allowed package names |
| -------------------- | ----------------- | --------------------- |
| L-M                  | AOSP    | `com.android.webview` **(default, preinstalled)** |
| L-M                  | Has GMS | `com.google.android.webview` **(default, preinstalled)** |
| N-P                  | AOSP    | `com.android.webview` **(default, preinstalled)** |
| N-P (TV/car devices) | Has GMS | `com.google.android.webview` **(default, preinstalled)** |
| N-P (other devices)  | Has GMS | `com.android.chrome` **(default, preinstalled)**<br>`com.chrome.beta`<br>`com.chrome.dev`<br>`com.chrome.canary`<br>`com.google.android.apps.chrome` **(only userdebug/eng)**<br>`com.google.android.webview` **(preinstalled)** (see [Important notes for N-P](build-instructions.md#Important-notes-for-N-P)) |
| >= Q                 | AOSP    | `com.android.webview` **(default, preinstalled)** |
| >= Q                 | Has GMS | `com.google.android.webview` **(default, preinstalled)**<br>`com.google.android.webview.beta`<br>`com.google.android.webview.dev`<br>`com.google.android.webview.canary`<br>`com.google.android.webview.debug` **(only userdebug/eng)**<br>`com.android.webview` **(only userdebug/eng)** |

`system_webview_apk` and `trichrome_webview_apk` use `com.android.webview` as
their package name by default. If your device allows this package name, continue
to the [next section](#removing-preinstalled-webview). Otherwise, you can change
the package name for either target by setting the `system_webview_package_name`
GN arg (ex. `system_webview_package_name = "com.google.android.webview"`).

`monochrome_public_apk`'s package name defaults to `org.chromium.chrome`.
Because this not accepted by any build of the OS, you'll want to change this
with the GN arg `chrome_public_manifest_package =
"com.google.android.apps.chrome"`, or choose `system_webview_apk` instead.

See [internal instructions][1] for the Google-internal variants of the build
targets (`system_webview_google_apk`, `monochrome_apk`,
`trichrome_webview_google_apk`).

*** note
**Note:** TV/car devices have a bug where the release key signed WebView is
preinstalled on all Android images, even those signed with dev-keys. Because
humans cannot access release keys (`use_signing_keys = true` provides "developer
test keys," not release keys), you must remove the preinstalled WebView (see
below).
***

### Removing preinstalled WebView

If WebView is preinstalled (under the chosen package name) in the device's
system image, you'll also need to remove the preinstalled APK (otherwise, you'll
see signature mismatches when installing). **You can skip this step** if
either of the following is true:

* You [chose a package name](#Changing-package-name) which is not marked as
  "(preinstalled)", **or**
* You have an L-M device with GMS and you're a Googler using the signing keys
  per [internal instructions][1].

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

### Important notes for N-P

_This functionality was added in N and removed in Q, so you should skip this if
your device is L-M or >= Q._

If you have an Android build from N-P (and, it uses the Google WebView
configuration), then the `com.google.android.webview` package will be disabled
by default. More significantly, installing a locally compiled APK won't work,
since the on-device WebViewUpdateService will immediately uninstall such
updates. To unblock local development, you can re-enable the WebView package and
disable this behavior from WebViewUpdateService with:

```shell
# Only necessary if you're using the 'com.google.android.webview' package name
adb shell cmd webviewupdate enable-redundant-packages
```

## Installing WebView and switching provider

For help connecting your Android device, see the [Chromium
instructions](/docs/android_build_instructions.md#Installing-and-Running-Chromium-on-a-device).

You can install a locally compiled APK like so (substitute `system_webview_apk`
with the chosen build target name):

```shell
# Install the APK
out/Default/bin/system_webview_apk install

# Only on N+: tell Android platform to load a WebView implementation from this APK
out/Default/bin/system_webview_apk set-webview-provider
```

<!--
  TODO(crbug.com/41454956): merge this and the other "Tip" when we
  document the Trichrome target in detail.
-->
*** promo
**Tip:** `out/Default/bin/trichrome_webview_apk install` will handle installing
all its dependencies (i.e., `trichrome_library_apk`), so you can interact with
this target the same as you would interact with any other WebView build target.
***

## Start running an app

See [Start running an app](quick-start.md#start-running-an-app) from the quick
start.

## Troubleshooting

Please see the [Troubleshooting](quick-start.md#troubleshooting) section in the
quick start.

_TODO(ntfschr): document cases here which could arise generally, but wouldn't
for the quick start._

[1]: http://go/clank-webview/build_instructions.md
[2]: https://groups.google.com/a/chromium.org/forum/#!forum/android-webview-dev
