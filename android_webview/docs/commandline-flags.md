# Commandline flags

## Can I apply commandline flags on my device?

*** note
**Note:** this guide only applies to userdebug/eng devices and emulators. Most
users and app developers **do not** have debuggable devices, and therefore
cannot follow this guide.

If you need to toggle flags on production Android devices, you can use [WebView
DevTools](developer-ui.md).
***

You can check which Android image you have on your device with the following:

```sh
# If you don't have `adb` in your path, you can source this file to use
# the copy from chromium's Android SDK.
$ source build/android/envsetup.sh

# If this outputs "userdebug" or "eng" then you can apply flags following this
# guide. If it outputs "user" then you can only use WebView DevTools.
$ adb shell getprop ro.build.type
userdebug
```

If the above outputs "user," then you have two options:

* Try using [WebView DevTools](developer-ui.md) to toggle flags (also works on
  userdebug and eng devices)
* Reflash your device or create a debuggable emulator (see [device
  setup](device-setup.md))

## Overview

WebView (and Chromium in general) has two different types of "commandline
flags:"

* **Commandline Switches** enable debugging functionality. These look like
  `--name-of-switch`.
* **Feature Flags** configure WebView to enable or disable an in-development
  project or enhancement. These look like `NameOfFeature`.

Before applying commandline flags, you should understand which specific flags
you want to flip and whether each is a Commandline Switch or Feature Flag.
Then follow the steps below.

## Applying Commandline Switches {#commandline-switches}

<!-- Note: keep this language consistent with the section below. Search for
"kill and restart" -->
WebView reads flags from a specific file on the device as part of the startup
sequence. Therefore, it's important to always **kill and restart the
WebView-based app** you're examining after modifying commandline flags to ensure
the flags are picked up.

WebView always looks for the same file on the device
(`/data/local/tmp/webview-command-line`), regardless of which package is the
[the WebView provider](prerelease.md).

### Python script (recommended)

The simplest way to set WebView flags is with the dedicated python script. This
works regardless of which package is the WebView provider. **Note:** this script
will overwrite any Commandline Switches or Feature Flags which had previously
been applied.

```sh
# Overwrite flags (supports multiple)
build/android/adb_system_webview_command_line --highlight-all-webviews --force-enable-metrics-reporting
# Clear flags
build/android/adb_system_webview_command_line ""
# Print flags
build/android/adb_system_webview_command_line
```

### Generated Wrapper Script

If you have a locally compiled APK, you may instead set flags using the
Generated Wrapper Script like so:

```sh
autoninja -C out/Default system_webview_apk
# Overwrite flags (supports multiple)
out/Default/bin/system_webview_apk argv --args='--highlight-all-webviews --force-enable-metrics-reporting'
# Clear flags
out/Default/bin/system_webview_apk argv --args=''
# Print flags
out/Default/bin/system_webview_apk argv
```

*** note
**Note:** this method is only supported for `system_webview_*` and
`trichrome_webview_*` targets. Be careful when using a `monochrome_*` target, as
this will write to Chrome browser's flags file, and WebView **will not pick up
these flags**. If using Monochrome, you should instead use the python script
mentioned above.
***

### Manual

Or, you can use the `adb` in your `$PATH` like so:

```sh
FLAG_FILE=/data/local/tmp/webview-command-line
# Overwrite flags (supports multiple). The first token is ignored. We use '_'
# as a convenient placeholder, but any token is acceptable.
adb shell "echo '_ --highlight-all-webviews --force-enable-metrics-reporting' > ${FLAG_FILE}"
# Clear flags
adb shell "rm ${FLAG_FILE}"
# Print flags
adb shell "cat ${FLAG_FILE}"
```

## Verifying flags are applied

You can add the `--webview-verbose-logging` flag, which tells WebView to dump
its full commandline and variations state to logs during startup. You can filter
device logs with:

```shell
adb logcat | grep -iE 'Active field trial|WebViewCommandLine'
```

## Applying Feature Flags

[Feature Flags](/base/feature_list.h) (also stylized as `base::Feature` or
`BASE_FEATURE`) are Chromium's mechanism for toggling off-by-default code paths.
While debugging flags are also off-by-default, Feature Flags typically guard
soon-to-launch product enhancements until they're tested enough for field trials
or public launch, at which point the Feature Flag is removed and the legacy code
path is no longer supported and removed from the codebase. On the other hand,
debugging flags don't "launch," as they're typically only helpful for debugging
issues.

WebView supports the same syntax for toggling Feature Flags as the rest of
chromium: `--enable-features=feature1,feature2` and
`--disable-features=feature3,feature4`. You can apply `--enable-features` and
`--disable-features` like any other flags, per the steps above. Please consult
[`base/feature_list.h`](/base/feature_list.h) for details. Examples of toggling
Feature Flags:

```sh
# Enable a single Feature Flag:
build/android/adb_system_webview_command_line --enable-features=WebViewTestFeature

# Enable multiple Feature Flags:
build/android/adb_system_webview_command_line --enable-features=WebViewTestFeature,WebViewBrotliSupport

# Enable and disable Feature Flags:
build/android/adb_system_webview_command_line --enable-features=WebViewTestFeature --disable-features=WebViewBrotliSupport

# Use Commandline Switches and Feature Flags at the same time:
build/android/adb_system_webview_command_line --highlight-all-webviews --enable-features=WebViewTestFeature
```

As with [Commandline Switches](#commandline-switches), we support multiple tools
for toggling Feature Flags (Python script, Generated Wrapper Script, manual).

<!-- Note: keep this language consistent with the section above. Search for
"kill and restart" -->
As before, it's important to **kill and restart the WebView-based app** you're
examining after modifying flags to ensure the flags are picked up.

## Finding Feature Flags and Commandline Switches

WebView supports toggling any Commandline Switches or Feature Flags supported in
any layer we depend on (//base, //content, etc.). For more details on Chromium's
layer architecture, see [this
diagram](https://www.chromium.org/developers/content-module) (replace "chrome"
with "android\_webview"). Although we support toggling these flags, not all
flags will have an effect when toggled, nor do we guarantee WebView functions
correctly when the flag is toggled.

Some interesting Commandline Switches:

 * `--highlight-all-webviews`: highlight the entire contents of all WebViews, to
   quickly identify which app content is rendered by a WebView vs. native Views.
 * `--show-composited-layer-borders`: highlight rendering layers, to identify
   possible graphics issues.
 * `--force-enable-metrics-reporting`: enable UMA metrics reporting (does not
   override app opt-out)
 * `--finch-seed-expiration-age=0 --finch-seed-min-update-period=0 --finch-seed-min-download-period=0 --finch-seed-ignore-pending-download`: always request a new finch seed when an app starts

WebView also defines its own Commandline Switches and Feature Flags:

 * C++ Commandline Switches are defined in
   [`aw_switches.cc`](/android_webview/common/aw_switches.cc). We use
   [`java_cpp_strings`](/docs/android_accessing_cpp_switches_in_java.md) to
   automatically generate Java switch constants from the C++ switches (see
   [`AwSwitches.java`](https://source.chromium.org/chromium/chromium/src/+/main:out/android-Debug/gen/android_webview/common_java/generated_java/input_srcjars/org/chromium/android_webview/common/AwSwitches.java)).
 * C++ Feature Flags are defined in
   [`aw_features.cc`](/android_webview/common/aw_features.cc). We use
   [`java_cpp_features`](/docs/android_accessing_cpp_features_in_java.md) to
   automatically generate Java constants from the C++ Feature Flags (see
   [`AwFeatures.java`](https://source.chromium.org/chromium/chromium/src/+/main:out/android-Debug/gen/android_webview/common_java/generated_java/input_srcjars/org/chromium/android_webview/common/AwFeatures.java)).

## Implementation

See [CommandLineUtil.java](https://cs.chromium.org/chromium/src/android_webview/java/src/org/chromium/android_webview/common/CommandLineUtil.java).
