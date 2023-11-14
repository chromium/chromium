# WebView for AOSP system integrators

[TOC]

## Overview

This guide is intended for anyone building and distributing
[AOSP](https://source.android.com) (e.g. Android device manufacturers or
maintainers of custom ROM images) who wishes to rebuild, update, modify, or
replace WebView in the system image for their Android device. This is not
intended for Chromium developers who simply wish to run their local build of
Chromium on a device.

Originally WebView was part of the Android framework, but since Android 5.0
(Lollipop) the WebView implementation has been provided by a separate APK. This
APK is preinstalled on the device and can be updated in the same ways as an
ordinary application.

The source code for the WebView implementation APK is maintained here, as part
of [the Chromium project](https://chromium.org). Building WebView from the AOSP
source tree (as earlier versions of Android did) is no longer supported.

*** aside
Unmodified retail Android devices cannot generally have their WebView replaced
or modified for security reasons; if you have a retail device this guide will
probably only be useful if you are building a custom ROM image.
***

## Prebuilt AOSP WebView

AOSP contains a prebuilt WebView APK for each supported CPU architecture, and
the appropriate APK will be included in the system image by default. These APKs
can be found in the
[external/chromium-webview](https://android.googlesource.com/platform/external/chromium-webview/)
directory in an AOSP checkout.

The prebuilt is provided in order to ensure that AOSP has a functional WebView
for development and testing purposes. It is not currently updated on a regular
schedule, and may have known security issues. It is strongly recommended that
AOSP system images which are being shipped to end user devices include a recent
stable version of WebView built following this guide, instead of the potentially
outdated prebuilt version.

## Building WebView for AOSP

*** promo
If you are not already familiar with building the Chromium browser for Android,
we recommend that you first follow
[the general guide for Chromium on Android](/docs/android_build_instructions.md)
to ensure that your computer and Chromium checkout are properly configured.

Make sure that you can build `chrome_public_apk`, install it on your device, and
use it before continuing, as troubleshooting issues with WebView can be more
difficult.
***

You will need to make several decisions before building WebView for AOSP:

### Choosing a WebView variant

There are currently three different variants of WebView that you can build, and
you will need to decide which one is appropriate for your device. All three have
the exact same features and app-facing behaviour, but are packaged in different
ways that can provide advantages in certain configurations.

More detailed background and technical information about the different variants
of WebView [is available here](webview-packaging-variants.md), but here's a
summary:

#### Standalone WebView

Most AOSP devices will use this variant. It is compatible with Android 5.0
(Lollipop) and later, and is the only variant which can be used on Android 5.x
(Lollipop) and 6.x (Marshmallow).

The standalone WebView is a single APK which contains the entire WebView
implementation. The prebuilt APK provided in AOSP is a standalone WebView APK.

The build target is called `system_webview_apk` and the resulting output file is
called `SystemWebView.apk`. The prebuilt APK provided in AOSP has been renamed
to `AndroidWebview.apk` for historical reasons, and the filename used in AOSP is
not significant; only the package name matters.

#### Monochrome

If your AOSP device will include a default web browser based on Chromium, it may
be beneficial to use Monochrome as the WebView implementation. Monochrome is
compatible with Android 7.x (Nougat), 8.x (Oreo) and 9.x (Pie), but not with
Android Q and later due to changes made to support Trichrome.

Monochrome is a single APK which contains both the entire WebView
implementation, and also an entire Chromium-based web browser. Since WebView and
the Chromium browser share a lot of common source code, the Monochrome APK is
much smaller than having a separate WebView APK and browser APK.

However, Monochrome can make it more difficult for you to allow the user to
freely choose their own web browser and can have other downsides: see
[this section](#Special-requirements-for-Monochrome) for more information.

The build target is called `monochrome_public_apk` and the resulting output file
is called `MonochromePublic.apk`.

#### Trichrome

Trichrome is only compatible with Android Q and later.

Trichrome is composed of three APK/AABs:

1. TrichromeWebView contains WebView-specific code and data, and provides
Android apps with the WebView implementation.

2. TrichromeChrome contains browser-specific code and data, and provides the
user with a Chromium-based web browser.

3. TrichromeLibrary contains the shared code and data, and is only used as an
internal implementation detail of TrichromeWebView and TrichromeChrome.

The three Trichrome APKs together are roughly the same size as Monochrome,
providing the same benefits, but many of the downsides and complexities of
Monochrome don't apply to Trichrome.

The build targets are called `trichrome_webview_apk`, `trichrome_chrome_bundle`,
and `trichrome_library_apk` respectively, and the resulting output files are
called `TrichromeWebView.apk`, `TrichromeChrome.aab`, and
`TrichromeLibrary.apk`.

### Choosing a WebView version

WebView follows the same branching and release model as the rest of the Chromium
project: a beta version is branched from the main branch approximately every
six weeks, and after approximately six weeks of beta testing it is released to
stable. If critical security or functionality issues are discovered after the
stable release, a new version may be released from the same stable branch at any
time (depending on urgency).

If you are intending to release your WebView build to users, you should
generally use a stable release tag - ideally the most recent stable release,
which includes the latest security and stability fixes. You can check the
current stable and beta version numbers using
[the Chromium dashboard](https://chromiumdash.appspot.com/releases?platform=Android).
See the "Syncing and building a release tag" section on
[this page](https://www.chromium.org/developers/how-tos/get-the-code/working-with-release-branches)
to check out the desired release tag.

If you're intending to build WebView just in order to develop, modify, or
customise it, it's usually best to work directly on the latest version of the
main branch. Chromium's main branch is covered by a large number of
automated build and test systems that ensure it is sufficiently stable for
development purposes at almost all times.

### Building WebView for a new or in-development version of Android {#pre-release}

If you want to build WebView for a version of Android which was recently
released or currently in development, you may find that the current stable
version in the public repository is not yet compatible with that version of
Android.

If this happens, you're likely to see errors referring to the `targetSdkVersion`
of the WebView APK, or about a class called
`WebViewChromiumFactoryProviderFor<version>` being missing. You can't fix these
problems by changing the `targetSdkVersion` or adding the missing class: this
will just cause difficult-to-diagnose issues later when the WebView is actually
used by applications that rely on newly introduced APIs.

At present, the changes required in WebView to support a new version of Android
are developed in a non-public repository, and we only release the WebView
changes after the source code for the new version of Android has been released.

For development and testing purposes, you can try a newer version of WebView
which may be compatible, but since newer versions have not yet been qualified as
stable they shouldn't generally be used in a shipping device. You can contact
the WebView team via the [android-webview-dev Google group][1] for guidance.

### Choosing build options

WebView is configured at build time using
[GN arguments](https://www.chromium.org/developers/gn-build-configuration). The
most important GN arguments to build a release WebView suitable for end users
are:

``` gn
target_os = "android"
target_cpu = "arm64"       # or "arm", "x86", or "x64"; see below

# Create an official release build. Only official builds should be distributed
# to users, as non-official builds are intended for development and may not
# be configured appropriately for production.
is_debug = false
is_official_build = true

# Use the default production settings for field trials, instead of the testing
# defaults.
disable_fieldtrial_testing_config = true

# WebView's efficient native library loading mechanism is not compatible with
# component builds of Chromium.
is_component_build = false

# Disable Google-specific branding/features
is_chrome_branded = false
use_official_google_api_keys = false

# May disable some experimental (unstable) features. Hides WebView DevTools
# (a debugging tool most users won't need to access).
android_channel = "stable"
```

The `target_cpu` option must be set to
[the CPU architecture which corresponds to your Android build](/docs/android_build_instructions.md#Figuring-out-target_cpu).
64-bit builds of WebView (for `arm64` or `x64`) include the code for both the
64-bit and corresponding 32-bit architecture, to support both 64-bit and 32-bit
applications. Any Android device which is able to run 64-bit applications
**must** use a 64-bit build: a WebView built for `arm` will not function
correctly on an `arm64` device.

*** note
The correct `target_cpu` may not be the actual CPU architecture of the hardware.
Some Android devices have a 64-bit CPU but run a 32-bit version of Android and
are not compatible with 64-bit applications. On these devices you should use a
32-bit version of WebView.
***

The `android_sdk_release` option should always be left as the default setting
for the version of the Chromium code you are using; do not specify a different
version. It is not necessary or beneficial to use an older SDK even if you are
building a WebView for an older Android version - the built WebView is fully
backward compatible, and building with older SDKs is not tested or supported.

#### Signing your WebView

By default the WebView APK will be signed with an insecure test key provided as
part of the public Chromium source code. For distribution to users, it should be
signed with a private key you control instead. Follow the
[general Android documentation](https://developer.android.com/studio/publish/app-signing#generate-key)
to create a keystore, and copy the keystore file into your Chromium checkout.
Configure the build to use this keystore with the following GN arguments:

``` gn
# Paths which begin with // are relative to the "src" directory.
default_android_keystore_path = "//my-keystore.keystore"
default_android_keystore_name = "my-key-alias"
default_android_keystore_password = "my-password"
```

#### Choosing a package name

The default Android package name for the standalone WebView is
`com.android.webview`, which AOSP is configured to use by default. If you plan
to distribute updates to your WebView via an app store or other update mechanism
outside of a system OTA update, then you may need to change this package name to
one of your own choosing, to avoid conflicting with other versions of WebView.
You can set a custom package name for the standalone WebView with the following
GN argument:

``` gn
# This is used as the Android package name and should follow normal Java/Android
# naming conventions.
system_webview_package_name = "com.mycompany.webview"
```

If you change the package name, you will need to
[reconfigure your Android build](#Configuring-the-Android-framework) to use the
new package name.

#### Proprietary codecs

In addition, you may want to include support for proprietary audio and video
codecs, as Google's WebView does. These codecs may be covered by patents or
licensing agreements, and you should seek legal advice before distributing a
build of WebView which includes them. You can enable them with the following GN
arguments:

``` gn
ffmpeg_branding = "Chrome"
proprietary_codecs = true
```

#### Crash stack unwinding

By default, WebView builds include unwind tables in the final APK. We recommend
keeping this default because it helps Android's default debuggerd process report
meaningful stack traces for crashes that occur inside WebView's native code.
This is how Google's WebView builds are configured.

If you choose to go against this recommendation, you may exclude unwind tables
from your WebView build to save some binary size:

``` gn
exclude_unwind_tables = true
```

#### Other build options

Other build options may be used but are not supported by the WebView team and
may cause build failures or problems at runtime. Many of the Chromium build
options do not affect WebView at all, so you should investigate the
implementation of any option you wish to change before assuming that it does
what you expect.

### Building WebView

See the [general WebView build instructions](build-instructions.md).

### Adding your WebView to the system image

The simplest way to add your own version of standalone WebView to the system
image is to copy the APK into the `external/chromium-webview` directory in your
AOSP checkout, replacing the existing prebuilt APK. If you configured your own
signing key when building WebView, you should edit
`external/chromium-webview/Android.mk` as follows:

``` sh
# replace the line:
# LOCAL_CERTIFICATE := $(DEFAULT_SYSTEM_DEV_CERTIFICATE)
# with:
LOCAL_CERTIFICATE := PRESIGNED
```

This will prevent the Android build system from resigning the APK with the
default platform key.

For Monochrome or Trichrome APKs you will need to define your own prebuilt
modules in a new `Android.mk` file. You may need to contact the WebView team via
the [android-webview-dev Google group][1] for help creating the correct build
files.

### Configuring the Android framework

#### Android 10.x (Q)

Using Monochrome as a WebView provider on Android 10 is not supported;
Chrome packages should not be included in the configuration as either the
Trichrome WebView or standalone WebView should be used.

The configuration mechanism for Android 10 is the same as the following section
(for Android 7-9), with the exception that the `isFallback` attribute no longer
causes the provider to be automatically disabled if another implementation is
available. Android 10 never automatically enables/disables WebView
implementations under normal usage.

Instead, the `isFallback` attribute is used to allow clean migration from an
older configuration. When a device is first booted with Android 10, any provider
marked as `isFallback` will be re-enabled for all users, as a one-time change.
This ensures that devices which previously used Chrome as their implementation
on Android 9 and had a disabled WebView do not end up with no enabled WebView
implementations.

Thus, if upgrading from an Android 9 device, it's recommended that you leave
`isFallback` set to true for any provider which had it set to true in the
Android 9 configuration. If this configuration is for a device which has never
used an older version of Android, `isFallback` is not necessary and can be
ignored.

#### Android 7.x (Nougat), 8.x (Oreo), and 9.x (Pie)

The permitted WebView implementations are configured using an XML file in the
framework. The default configuration file is located at
`frameworks/base/core/res/res/xml/config_webview_packages.xml` - you can either
edit this file in place, or create a new configuration file for your product and
include it as a resource overlay using the `PRODUCT_PACKAGE_OVERLAYS` build
variable.

There must be at least one provider defined in the configuration. If more than
one provider is defined, they will be considered in the order listed in the
file, and the first valid provider chosen by default. A menu is provided in the
Android developer settings UI to allow the user to choose a different provider.

You can print the base64-encoded signature of a compiled APK with the following
(look for `Full Signature:` in the output):

```shell
# For an APK or Bundle target compiled from chromium (replace
# "system_webview_apk" with your build target):
$ out/Default/bin/system_webview_apk print-certs --full-cert

# For a pre-compiled APK or Bundle:
$ build/android/apk_operations.py print-certs --full-cert \
  --apk-path /path/to/AndroidWebview.apk
```

*** note
On `userdebug` and `eng` builds of Android, the WebView's signature,
preinstallation, and version code checks are not performed, to simplify
development. Make sure to test your configuration using a `user` build of
Android to ensure that it will work as intended for users.
***

Here's a commented example XML file:

``` xml
<?xml version="1.0" encoding="utf-8"?>
<webviewproviders>

  <!-- Each webviewprovider tag has the following attributes:

      packageName (required): The Android package name of the APK.

      description (required): The name shown to the user in the developer
          settings menu.

      availableByDefault (default false): If true, this provider can be
          automatically selected by the framework, if it's the first valid
          choice. If false, this provider will only be used if the user selects
          it themselves from the developer settings menu.

      isFallback (default false): If true, this provider will be automatically
          disabled by the framework, preventing it from being used or updated
          by app stores, unless there is no other valid provider available.
          Only one provider can be a fallback. See "Special requirements for
          Monochrome" to understand one possible use case.

      Each webviewprovider tag can also contain zero or more signature tags as
      children. If the provider has no signature tags, then the provider must
      be preinstalled (or be an installed update to a preinstalled provider) to
      be considered valid. If at least one signature tag is specified, then the
      provider is considered valid if it is signed with any one of the given
      signatures.

      Each signature tag contains the entire public certificate corresponding
      to the private key used to sign the APK, encoded as base64. See the
      documentation above for instructions to print the signature of an APK in
      the correct format. -->


  <!-- This provider is listed first and has "availableByDefault" set to true,
       so will be used as the default if it's valid. Because it does not have a
       signature specified, it must be preinstalled. -->
  <webviewprovider packageName="com.android.webview" description="AOSP WebView"
                   availableByDefault="true">
  </webviewprovider>

  <!-- This provider will not be used unless the user chooses it from the
       developer settings menu. It must be signed with the correct key but
       does not have to be preinstalled. -->
  <webviewprovider packageName="com.android.webview.beta"
                   description="Beta WebView">
    <signature>MIIFxzCCA6+gAw ... FdCQ==</signature>
  </webviewprovider>

  <!-- This provider will be disabled automatically, and will not receive
       updates from app stores, unless no other provider is valid. -->
  <webviewprovider packageName="com.android.webview.fallback"
                   description="Fallback WebView" isFallback="true">
  </webviewprovider>
</webviewproviders>
```

#### Android 5.x (Lollipop) and 6.x (Marshmallow)

The name of the WebView package is specified as a string resource in the
framework. The default value is located in
`frameworks/base/core/res/res/values/config.xml` under the resource name
`config_webViewPackageName` - you can either edit this file in place, or create
a new configuration file for your product and include it as a resource overlay
using the `PRODUCT_PACKAGE_OVERLAYS` build variable.

## Making your WebView updatable

In order to allow your WebView implementation to be updated without requiring a
full system OTA update, you need several things:

1. **Secure signing keys.** Your WebView APK must be signed with a key that you
generated and keep safe, [as described above](#Signing-your-WebView). If this
key were to be compromised, an attacker could potentially trick users into
installing a malicious version of WebView on their device, affecting all apps
which use WebView.

2. **A unique package name.** Your APK should
[have a package name](#Choosing-a-package-name) which refers to your
company/organisation, to differentiate it from other versions of WebView. You
should follow the usual Java package naming conventions, using a domain name you
control in reverse order.

3. **A distribution mechanism.** WebView is a normal APK, so can be installed
onto a device by any mechanism that can install APKs. You might distribute
updates by publishing them in an Android app store, by using a custom updater
specific to your Android build which downloads the APK directly, or by allowing
users to download the APK themselves and install it via sideloading (though this
probably should only be used for development/test versions). Ideally, your
distribution mechanism should update WebView automatically without user
intervention, to ensure that users receive the latest security updates.

## Special requirements for Monochrome

Standalone WebView is not generally visible to the user unless they go looking
for it, since it's a system app with no UI. In contrast, Monochrome is a web
browser as well as a WebView implementation, which makes it much more visible,
since it's a normal app with a launcher icon.

This means that the user may wish to be able to disable Monochrome in some
circumstances; for example, if they prefer to use a different web browser.
Allowing the user to do this without breaking applications which use WebView can
be difficult - on current versions of Android, WebView cannot function correctly
if the package is disabled, and app stores generally do not update disabled
apps, so the user would not receive security updates. An alternate WebView
implementation must be provided on the device to allow users to disable
Monochrome without unwanted side effects.

The simplest option is to also preinstall the standalone WebView, and mark it as
`isFallback="true"` in the framework WebView configuration. The framework will
then automatically disable the standalone WebView in most cases, and will only
re-enable it if the user does in fact disable Monochrome. This will allow the
implementation that the user is using to receive updates, while blocking
unnecessary updates of the implementation that is not being used.

However, preinstalling both Monochrome and the standalone WebView takes up a lot
of space, as they're both quite large. To avoid this, it's possible to
preinstall a "stub" version of the standalone WebView instead, which is much
smaller, but still performs the function of a full WebView APK by using a
special codepath in the framework: it loads its actual implementation from the
preinstalled Monochrome APK at runtime. The stub and Monochrome must be matching
versions for this to work - they should be built together in a single build,
and any time you build a new version of Monochrome you should also rebuild the
stub.

Currently, the public Chromium source code does **not** contain a suitable stub
WebView implementation. Please [contact the WebView team][1] for assistance if
you're planning to ship a configuration based on Monochrome.

## Frequently asked questions

### Why are there security restrictions on which apps can be used as a WebView implementation?

When an application uses WebView, the WebView implementation code is loaded
directly into that app's process. This means that the WebView code has access to
all of that app's data, both in memory and on disk, and can make use of any of
that app's Android permissions. A malicious WebView implementation APK would
therefore be able to compromise the security of any app on the device which uses
WebView.

To mitigate this risk, the AOSP framework code only allows the WebView
implementation APK(s) specified by the AOSP system integrator to be used.

[1]: https://groups.google.com/a/chromium.org/forum/#!forum/android-webview-dev
