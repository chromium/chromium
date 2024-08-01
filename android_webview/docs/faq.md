# FAQ for WebView Users

[TOC]

## What is WebView?

WebView is a [system component of Android][1] which enables the apps you use to
show content from the web. Most apps you use every day use WebView in some way.

## How can I give feedback or report a bug?

Let us know what you think and help improve WebView for everyone on Android.
Any WebView-related bugs can be filed
[here](https://issues.chromium.org/issues/new?component=1456456&template=1923373).

## How can I contact the WebView development team?

You can reach out to the team through the [android-webview-dev Google group][2].

## Why do I need to update WebView?

WebView needs regular security updates just like your browser. We release a new
version every 6 weeks to make sure you stay safe while using apps on your phone.

## What’s the relationship between WebView and Chrome?

WebView is built on top of the open source Chromium project, but it doesn’t
share any data with Google Chrome.

In Android 7, 8, and 9 (Nougat/Oreo/Pie), WebView is built into Chrome. Because
they share so much underlying code, this saves space and memory on your device.
They still don’t share any data, however, and you can disable Google Chrome at
any time without impairing your device. When Chrome is disabled, WebView will
switch to a standalone version which isn't combined with Chrome.

In Android 10 (Q), WebView and Chrome still share most of their code to save
space and memory on your device, but now simply appear as two separate apps and
there is no longer any special behaviour when disabling Chrome.

## Are Chrome features like Sync or Data Saver available in WebView?

No. Although WebView and Chrome share a package in Android N, O, and P, they
don’t share data and Chrome-specific features like Sync and Data-Saver aren’t
available inside of WebView.

## What happens if I disable WebView?

We don't recommend that you disable WebView on your device. Apps which use
WebView are likely to crash or malfunction, and you won't receive important
security updates.

If WebView is already disabled on your device and cannot be enabled, that is
normal: when Chrome is being used as the WebView implementation, the separate
WebView package is automatically disabled to avoid downloading redundant
updates. You never need to manually enable or disable WebView.

[1]: https://developer.android.com/reference/android/webkit/WebView.html
[2]: https://groups.google.com/a/chromium.org/forum/#!forum/android-webview-dev
