# Device Setup for WebView development

[TOC]

As WebView is an Android system component (rather than just an app), WebView
imposes additional requirements on the developer workflow. In particular,
WebView requires a physical device or emulator with a `userdebug` or `eng`
Android image. WebView doesn't support development on `user` builds.

To build WebView, or [run WebView's automated tests](./test-instructions.md),
you'll need to set up either an emulator or a physical device.

## I have a device or emulator. Will it work for development?

You can check which Android image you have on your device with the following:

```sh
# If you don't have `adb` in your path, you can source this file to use
# the copy from chromium's Android SDK.
$ source build/android/envsetup.sh

# If this outputs "userdebug" or "eng" then you're OK! If this outputs "user"
# then you must reflash your physical device or configure a new emulator
# following this guide.
$ adb shell getprop ro.build.type
userdebug
```

## Emulator (easy way)

*** promo
Unless you have a hardware-specific bug, or need to use a pre-release Android
version, a physical device is usually unnecessary. An `x86` emulator should be
easier to setup.
***

You can generally follow chromium's [Android
emulator](/docs/android_emulator.md) instructions. You should choose a **Google
APIs** image. The AOSP-based image will also work, but imposes additional
developer hurdles. Note that you shouldn't use a **Google Play** image for
development purposes because they are `user` builds, see [Why won't a user
image work](#why-won_t-a-user-image-work) below.


## Physical device

### Flash a prebuilt image

Googlers can consult internal instructions
[here](http://go/clank-webview/device_setup.md).

External contributors can flash a prebuilt `userdebug` image (based off
aosp-main) onto a Pixel device with [Android Flash
Tool](https://flash.android.com/welcome?continue=%2Fcustom). This requires a
browser capable of WebUSB (we recommend the latest Google Chrome stable
release).

### Building AOSP yourself (hard way)

*** note
This takes significantly longer than the two previous methods, so please
strongly consider one of the above first.
***

**Prerequisite:** a machine capable of [building
Android](https://source.android.com/source/building.html).

Clone an AOSP checkout, picking a branch supported for your device (you'll need
a branch above 5.0.0) from the [list of
branches](https://source.android.com/setup/start/build-numbers.html#source-code-tags-and-builds):

```shell
mkdir aosp/ && cd aosp/ && \
  repo init -u https://android.googlesource.com/platform/manifest -b android-9.0.0_r33 && \
  repo sync -c -j<number>
```

You can obtain binary drivers for Nexus/Pixel devices
[here](https://developers.google.com/android/drivers). Drivers should match your
device and branch. Extract and run the shell script:

```shell
# Change the filenames to match your device/branch (this uses "crosshatch" as an
# example)
tar -xvzf /path/to/qcom-crosshatch-pd1a.180720.030-bf86f269.tgz
./extract-qcom-crosshatch.sh # Extracts to the vendor/ folder
```

You can build AOSP and flash your device with:

```shell
source build/envsetup.sh
device="crosshatch" # Change this depending on your device hardware
lunch aosp_${device}-userdebug
make -j<number>

# Flash to device
adb reboot bootloader
fastboot -w flashall
```

For more information, please defer to [official
instructions](https://source.android.com/setup/build/downloading).

## Why won't a user image work?

`user` images have all of Android's security features turned on (and they can't
be disabled). In particular, you won't be able to install a locally built
WebView:

* Most `user` images are `release-keys` signed, which means local WebView builds
  can't install over the preinstalled standalone WebView. This blocks
  development on L-M, since this is the only WebView provider.
* On N+, although you _can install_ a locally compiled
  `monochrome_{public_}apk`, this is not a valid WebView provider. Unlike on
  `userdebug`/`eng` images, the WebView update service performs additional
  signature checks on `user` images, only loading code that has been signed by
  one of the expected signatures—as above, these keys are not available
  for local builds.

Both of the above are important security features: these protect users from
running malware in the context of WebView (which runs inside the context of
apps).
