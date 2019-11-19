# Commandline flags

## Applying flags

*** note
**Note:** this requires either a `userdebug` or `eng` Android build (you can
check with `adb shell getprop ro.build.type`). Flags cannot be enabled on
production builds of Android.
***

WebView reads flags from a specific file on the device as part of the startup
sequence. Therefore, it's important to always **kill the WebView-based app**
you're examining after modifying commandline flags, to ensure the flags are
picked up during the next app restart.

WebView always looks for the same file on the device
(`/data/local/tmp/webview-command-line`), regardless of which package is the
[the WebView provider](prerelease.md).

### Python script

The simplest way to set WebView flags is with the dedicated python script. This
works regardless of which package is the WebView provider:

```sh
# Overwrite flags (supports multiple)
build/android/adb_system_webview_command_line --show-composited-layer-borders --webview-log-js-console-messages
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
out/Default/bin/system_webview_apk argv --args='--show-composited-layer-borders --webview-log-js-console-messages'
# Clear flags
out/Default/bin/system_webview_apk argv --args=''
# Print flags
out/Default/bin/system_webview_apk argv
```

*** note
**Note:** be careful if using a `monochrome_*` target, as the Generated Wrapper
Script writes to Chrome browser's flags file, and WebView **will not pick up
these flags**. If using Monochrome, you can set flags with the
`system_webview_*` Generated Wrapper Scripts, or use one of the other methods
in this doc.
***

### Manual

Or, you can use the `adb` in your `$PATH` like so:

```sh
FLAG_FILE=/data/local/tmp/webview-command-line
# Overwrite flags (supports multiple). The first token is ignored. We use '_'
# as a convenient placeholder, but any token is acceptable.
adb shell "echo '_ --show-composited-layer-borders --webview-log-js-console-messages' > ${FLAG_FILE}"
# Clear flags
adb shell "rm ${FLAG_FILE}"
# Print flags
adb shell "cat ${FLAG_FILE}"
```

## Verifying flags are applied

You can confirm you've applied commandline flags correctly by dumping the full
state of the commandline flags with the [WebView Log Verbosifier
app](/android_webview/tools/webview_log_verbosifier/README.md) and starting up a
WebView app.

## Applying Features with flags

WebView supports the same `--enable-features=feature1,feature2` and
`--disable-features=feature3,feature4` syntax as the rest of Chromium. You can
use these like any other flag. Please consult
[`base/feature_list.h`](https://cs.chromium.org/chromium/src/base/feature_list.h)
for details.

## Interesting flags

WebView supports any flags supported in any layer we depend on (ex. content).
Some interesting flags and Features:

 * `--show-composited-layer-borders`
 * `--webview-log-js-console-messages`

WebView also defines its own flags and Features:

 * [AwSwitches.java](https://cs.chromium.org/chromium/src/android_webview/java/src/org/chromium/android_webview/AwSwitches.java)
   (and its [native
   counterpart](https://cs.chromium.org/chromium/src/android_webview/common/aw_switches.h))
 * [AwFeatureList.java](https://cs.chromium.org/chromium/src/android_webview/java/src/org/chromium/android_webview/AwFeatureList.java)
   (and its [native
   counterpart](https://cs.chromium.org/chromium/src/android_webview/common/aw_features.h))

## Implementation

See [CommandLineUtil.java](https://cs.chromium.org/chromium/src/android_webview/java/src/org/chromium/android_webview/common/CommandLineUtil.java).
