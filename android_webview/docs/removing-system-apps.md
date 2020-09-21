# Removing system apps

<!--
  Ideally this would just be a "details" element on build-instructions.md, but
  gitiles markdown does not support this tag.
-->

Removing a system app isn't WebView-specific, but it's occasionally necessary
for WebView development.

We have an [automated script to remove the preinstalled
WebView](build-instructions.md#Removing-preinstalled-WebView) from the device,
but if this script does not work or you need to remove a different system app,
you can manually run the following adb commands. This uses
"com.google.android.webview" as an example, but change the package name as
necessary for your case.

```sh
# Uninstall updates. Repeat "adb uninstall" until it fails with the
# "DELETE_FAILED_INTERNAL_ERROR" error message to make sure you've removed all
# the updates.
$ adb uninstall com.google.android.webview
Success
$ adb uninstall com.google.android.webview
Failure [DELETE_FAILED_INTERNAL_ERROR]

# Figure out the path of the system app. This varies depending on OS level.
$ adb shell pm path com.google.android.webview
package:/system/app/WebViewGoogle/WebViewGoogle.apk
$ adb root

# This is necessary for M and up:
$ adb disable-verity
$ adb reboot
# wait for device to reboot...
$ adb root

# For all OS versions:
# Mount the system partition as read-write and 'rm' the path we found before.
$ adb remount
$ adb shell stop
$ adb shell rm -rf /system/app/WebViewGoogle/WebViewGoogle.apk
$ adb shell start
```
