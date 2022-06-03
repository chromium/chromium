# Testing Chrome Custom Tabs with Chromium

[TOC]

Chrome Custom Tabs (CCT) allow an app to customize how Chrome looks and feels.
It gives an app more control over the web experience without having to use
WebView.

## What is Chrome Custom Tabs?

For more information on Chrome Custom Tabs, refer to
[this blog post](https://developer.chrome.com/multidevice/android/customtabs).
If you want to use Chrome Custom Tabs in your own app, the instructions are
[here](/third_party/custom_tabs_client/src/Using.md).

## Building

These instruction assume that you have already built Chromium for Android. If
not, instructions for building Chromium for Android are
[here](/docs/android_build_instructions.md). Details below assume that the
build is setup in out/Default.

### Build Chrome Custom Tabs example

```shell
$ autoninja -C out/Default custom_tabs_client_example_apk
```

### Install Chrome Custom Tabs example

```shell
# Install the example
$ out/Default/bin/custom_tabs_client_example_apk install
```

## Start running the app

**That it!** The example app should be installed and available. Once you
launch "Chrome Custom Tabs Example" ("Chrome C..." in apps), you should be
able to switch to use Chromium by changing "Package" to Chromium (or any
version of Chrome installed on the device). Then simply click "Launch URL
in a Chrome Custom Tab"
