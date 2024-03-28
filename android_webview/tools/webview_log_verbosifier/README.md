# WebView Log Verbosifier

*** note
**Deprecated:** The Log Verbosifier app does not work on Android R. M84 is the
last milestone to support this for other OS levels.

Instead, use the **webview-verbose-logging** flag in [WebView
DevTools](/android_webview/docs/developer-ui.md) ([added in
M83](https://chromiumdash.appspot.com/commit/6f015ed47dd2e63b683c8fed6fece7a9ea16f824)).
This flag behaves exactly the same as if the Log Verbosifier app is installed,
but will be compatible with all OS levels (including Android R). The log format
is identical, so you can [search logcat](#Searching-logcat) as before.
***

WebView Log Verbosifier is an empty app (in fact, it cannot be launched).
However, if this app is installed, WebView will log the active field trials and
CommandLine flags, for debugging/QA purposes. An empty app is used because it can
be installed on any device (including user builds, where field trials are still
relevant).

## Build and install

We no longer support building the log verbosifier from source. Googlers can get
a [prebuilt copy of the log
verbosifier](http://go/clank-webview-legacy/zzarchive/webview-manual-testing).
External contributors can request a precompiled copy by [emailing the WebView
team](https://groups.google.com/a/chromium.org/forum/#!forum/android-webview-dev).

## Searching logcat

You can `grep` the logcat like so:

```shell
adb logcat | grep -i 'Active field trial' # Field trials, one per line
adb logcat | grep -i 'WebViewCommandLine' # CommandLine switches, one per line
adb logcat | grep -iE 'Active field trial|WebViewCommandLine' # Both
```

Then just start up any WebView app.

## Uninstalling

When you're done investigating flags/field trials, you can disable the logging
by uninstalling the app:

```shell
adb uninstall org.chromium.webview_log_verbosifier
```

## See also

* [How to set commandline flags in
  WebView](/android_webview/docs/commandline-flags.md)
