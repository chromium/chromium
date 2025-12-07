# Reporting bugs in WebView

Thanks for your interest in reporting a bug with Android WebView! We have some
tips and guidelines for how you can file your bug report to help us quickly
diagnose your bug.

*** promo
You can file WebView bugs at
https://issues.chromium.org/issues/new?component=1456456&template=1923373, but
please continue reading to understand the best practices for reporting WebView
bugs.
***

[TOC]

## Reproducible in Google Chrome browser {#repro-in-chrome}

If a bug can reproduce in Google Chrome browser, it's best to file this as a
Chrome bug, not as a WebView bug. This is true even if the bug also reproduces
in both Chrome and WebView. Please file this at https://crbug.com/wizard so that
the Chrome team can properly triage this.

## Capture a bug report zip file {#bug-report-zip}

It's ideal if you can capture a **bug report zip file** on the repro device
right after you reproduce the bug. See [Capture and read bug
reports](https://developer.android.com/studio/debug/bug-report) for instructions
on how to automatically generate this zip file on Android. Please attach the zip
file when you file your bug.

This zip file includes the standard set of device logs (also known as `adb
logcat`), but it also includes several other debug information which is useful
for diagnosing the issue.

## Highlight WebViews {#highlight}

![WebView flag UI](images/webview_flag_ui.png)

There's a debug flag you can turn on which will highlight all WebViews with a
yellow color. This is useful for bug reports because it helps us confirm if
WebView is the culprit or if it's a different web technology which is causing
the bug.

You can enable this with 3 steps:

1. Launch WebView DevTools. The easiest way of doing this is with an adb
   command:

   ```shell
   adb shell am start -a "com.android.webview.SHOW_DEV_UI"
   ```

   If you don't have adb installed or can't use it for some reason, then consult
   [WebView DevTools user guide](./developer-ui.md) for other ways to launch.
2. Tap the "Flags" option in the bottom navigation bar. Select
   **highlight-all-webviews** and toggle this to **Enabled.**
3. Kill your other apps and restart them. They will now highlight all WebViews
   in yellow.

To undo this, swipe down from the top of your screen to find a notification from
WebView DevTools. You can tap this notification to be taken back to WebView
DevTools where you can tap the **Reset flags** button at the top of the screen.

## Record a video {#screenrecord}

If you can reproduce the bug, then please try to record a video showing the
repro steps. Here's the quick steps for doing this:

1. Please enable highlight-all-webviews so we can clearly see where the WebViews
   are when you repro the bug (see the previous section for instructions).
2. Newer Android versions have a builtin screen record option. To use this
   option, swipe down twice from the top of your screen.
3. Tap the **Screen record** button. You might need to swipe right to find this
   button. If you cannot find the option, then please see [Take a screenshot or
   record your screen on your Android
   device](https://support.google.com/android/answer/9075928?hl=en) for full
   instructions.
4. Choose what you want to record, enable the **show touches** option, and tap
   **Start.**
5. When you're done reproducing the issue, swipe down again from the top of the
   screen and tap the Screen recorder notification to stop recording.
6. Attach the mp4 video file when you file your bug report.

**Alternative instructions:** If you cannot find the screen record option, then
you can use the `adb` commandline tool instead:

1. Enable [developer
   options](https://developer.android.com/studio/debug/dev-options). The usual
   way to do this is go into Settings > About phone > Build number > tap the
   **Build Number** seven times until you see the message "You are now a
   developer!"
2. Return to the previous screen in the settings app to find **Developer
   options** at the bottom.
3. Scroll through developer options until you find the [**Input**
   section](https://developer.android.com/studio/debug/dev-options#input). Turn
   on the **Show taps** option.
4. Connect your device to your computer with a USB cable. Record a video with
   `adb shell screenrecord /sdcard/demo.mp4`. Start reproducing the bug on your
   device. Press `Control + C` to stop recording when you're done. Run
   `adb pull /sdcard/demo.mp4` to pull the video file to your computer.
5. Attach the mp4 video file when you file your bug report.

## Create a minimal sample app {#sample-app}

If you can create a minimal sample app to reproduce the bug, this is usually
very helpful at resolving your issue quickly. To help us work quickly, we need
**two files** from you:

1. A compiled APK file, **and**
2. A zip file of source code (`.zip` format is preferred, please don't use 7zip
   or other archive formats)

Please attach **both** files to the bug report.

**Tip:** if your bug also reproduces on a real app, please mention this in the
bug report as well. A sample app is still helpful, but knowing that this affects
real apps will help us prioritize your report appropriately.

## Reproducing bugs which require signing into app accounts {#test-account}

If a bug occurs in apps which require signing into an account, then you will
either need to provide a [minimal sample app](#sample-app) which does not
require sign-in credentials, or you will need to share a test account for us to
reproduce with. The minimal sample app is always preferred.

If you decide to share sign-in credentials, then please let us know on the bug
before you share the credentials. To share credentials, you can either attach
this in Google Doc and share the link on the bug (members of our team will
request access with our @google.com accounts) or you can ask us to restrict the
entire bug to **limited visibility + Google** so that you can share the
username/password in a bug comment. Please wait to share username/password until
a member of our team has confirmed the bug is restricted.
