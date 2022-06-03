# WebView Providers

Since Android Lollipop, WebView has been an updateable system component, with
the updateable part of the implementation distributed as an APK or App Bundle.
We refer to this updateable package as the "WebView provider" on the device.

[TOC]

## WebView provider options

Some OS images permit users to change their WebView provider to a non-default
package while other images only support a single preinstalled default. This
table captures the WebView provider options for the most common device
configurations:

<!-- Keep this table in sync with build-instructions.md and the table below -->
| API level            | Has GMS vs. AOSP? | Allowed apps |
| -------------------- | ----------------- | ------------ |
| L-M                  | AOSP    | Standalone AOSP WebView **(default, preinstalled)** |
| L-M                  | Has GMS | Standalone Google WebView **(default, preinstalled)** |
| N-P                  | AOSP    | Standalone AOSP WebView **(default, preinstalled)** |
| N-P (TV/car devices) | Has GMS | Standalone Google WebView  **(default, preinstalled)** |
| N-P (other devices)  | Has GMS | Monochrome Stable **(default, preinstalled)**<br>Monochrome Beta<br>Monochrome Dev<br>Monochrome Canary<br>Monochrome (no channel) **(only userdebug/eng)**<br>Google WebView Stub **(preinstalled)** or Standalone Google WebView (see [Important notes for N-P](build-instructions.md#Important-notes-for-N-P)) |
| >= Q                 | AOSP    | Standalone AOSP WebView **(default, preinstalled)** |
| >= Q                 | Has GMS | Trichrome WebView Google Stable **(default, preinstalled)**<br>Trichrome WebView Google Beta<br>Trichrome WebView Google Dev<br>Trichrome WebView Google Canary<br>Trichrome WebView Google (no channel) **(only userdebug/eng)**<br>Standalone AOSP WebView or Trichrome WebView AOSP **(only userdebug/eng)** |

*** aside
[Vendors modifying AOSP][aosp] can configure which providers are compatible with
their OS image, although they often stick with the default configuration (marked
above as "AOSP").

Vendors shipping OS images which include GMS and the Play Store must use
Google's provided WebView configuration (marked above as "Has GMS"). This is to
ensure Google can deliver WebView updates to users, and we enforce this with GTS
tests. Most production Android devices use this configuration.
***

### The currently selected WebView provider

Most devices use the **default** option listed in the table above. If your
device supports multiple options, you can figure out which is currently selected
by:

* Looking in [developer settings](prerelease.md#switch-channel)
* With the [commandline](quick-start.md#Troubleshooting) (look for "Current
  WebView package," only works on O+)

### Switching WebView provider

On Nougat and above, you can switch WebView providers [in developer
settings](prerelease.md#switch-channel) or in your terminal:

```sh
# If you're building an app locally (change "system_webview_apk" as desired):
$ autoninja -C out/Default system_webview_apk
$ out/Default/bin/system_webview_apk install
$ out/Default/bin/system_webview_apk set-webview-provider

# Or, if you've already installed the app (change "com.android.webview" as
# desired):
$ adb shell cmd webviewupdate set-webview-implementation com.android.webview
```

## WebView provider requirements

A package must fulfill several requirements to be eligible to be a WebView
provider. These requirements are enforced by the **WebView Update Service**,
which runs as part of the Android framework on the device.

### Installed and enabled

On Android O+, an eligible WebView provider must be installed and enabled for
**all user profiles** (some Android features are implemented behind the scenes
with [multiple user profiles](prerelease.md#multiple-profiles)). On Android L-N,
the package only needs to be installed and enabled for the default user profile.

If you uninstall (or disable) the selected WebView provider, the WebView Update
Service will fallback to a different package based on an ordered preference (the
order is [predetermined in the OS image][aosp]). If there are no more eligible
packages (if this was the only package or the user disabled/removed all other
packages), WebView will simply not work and WebView-based apps will crash until
the user re-enables one of the packages.

On Android N-P, `com.google.android.webview` and `com.android.chrome` are
mutually exclusive, due to "fallback logic." Disabling (or enabling) Chrome will
enable (or disable) the WebView stub. See [Important notes for
N-P](build-instructions.md#Important-notes-for-N-P) for more information.

### Package name

For security reasons, Android will only permit a predetermined list of package
names to act as WebView provider. The WebView team provides several different
preset lists, depending how the Android image will be configured.

<!-- Keep this table in sync with build-instructions.md and the table above -->
| API level            | Has GMS vs. AOSP? | Allowed package names |
| -------------------- | ----------------- | --------------------- |
| L-M                  | AOSP    | `com.android.webview` **(default, preinstalled)** |
| L-M                  | Has GMS | `com.google.android.webview` **(default, preinstalled)** |
| N-P                  | AOSP    | `com.android.webview` **(default, preinstalled)** |
| N-P (TV/car devices) | Has GMS | `com.google.android.webview` **(default, preinstalled)** |
| N-P (other devices)  | Has GMS | `com.android.chrome` **(default, preinstalled)**<br>`com.chrome.beta`<br>`com.chrome.dev`<br>`com.chrome.canary`<br>`com.google.android.apps.chrome` **(only userdebug/eng)**<br>`com.google.android.webview` **(preinstalled)** (see [Important notes for N-P](build-instructions.md#Important-notes-for-N-P)) |
| >= Q                 | AOSP    | `com.android.webview` **(default, preinstalled)** |
| >= Q                 | Has GMS | `com.google.android.webview` **(default, preinstalled)**<br>`com.google.android.webview.beta`<br>`com.google.android.webview.dev`<br>`com.google.android.webview.canary`<br>`com.google.android.webview.debug` **(only userdebug/eng)**<br>`com.android.webview` **(only userdebug/eng)** |

*** aside
The package name list can be [configured in AOSP][aosp].
***

### Signature (for user builds)

For security reasons, Android also checks the signature of WebView providers,
only permitting apps signed with the expected release keys.

This requirement is waived on userdebug/eng devices so we can install local
WebView builds (which don't have release keys) on test devices.

*** aside
The signatures can be [configured in AOSP][aosp].
***

### targetSdkVersion

A valid WebView provider must implement all the APIs exposed in that version of
the Android platform, otherwise calling a new API will crash at runtime. WebView
Update Service can't reliably determine which APIs a provider implements, so we
decided to use `targetSdkVersion` as a proxy for this:

* For a finalized Android version, a valid WebView provider must declare
  a `targetSdkVersion` greater than or equal to the platform's
  [`Build.VERSION.SDK_INT`](https://developer.android.com/reference/android/os/Build.VERSION#SDK_INT)
  value.
* For a pre-release (AKA in-development) Android version, a valid WebView
  provider must declare a `targetSdkVersion` equal to
  [`Build.VERSION_CODES.CUR_DEVELOPMENT`](https://developer.android.com/reference/android/os/Build.VERSION_CODES#CUR_DEVELOPMENT)
  and be compiled with the corresponding pre-release SDK.

In the Chromium repo, we configure this in GN args by setting
`android_sdk_release = "x"`, where "x" is the lowercase codename letter of the
desired OS version. Upstream chromium code usually only supports the latest
public Android version, so you should use that value for all public Android OS
versions. Googlers building with the internal repository may be able to override
this to target the current pre-release Android version.

*** note
**Note:** it is not sufficient to simply change `targetSdkVersion` in the APK:
new API calls will still crash at runtime! You should only configure this with
`android_sdk_release = "x"`, as this also pulls in the code to implement new
Android APIs. See [WebView for AOSP system
integrators](aosp-system-integration.md#pre-release) for details.
***

### versionCode

We enforce a minimum `versionCode` both for security (to prevent downgrade
attacks) and correctness (this ensures the package can implement all the new
WebView APIs in the new version of the OS). This is computed at runtime by the
WebView Update Service by taking the minimum of all valid WebView providers
installed on the system image.

You generally should not hit this issue for local builds, but may see this if
you're trying to install a really old WebView official build.

### Declare a native library

Because WebView is implemented partially in C++, the Android framework must load
its native library. On L, the native library must be called
`libwebviewchromium.so`. Starting with M and above, the native library must be
declared by the `com.android.webview.WebViewLibrary` metadata tag in
`AndroidManifest.xml`. See [Loading native code with RELRO
sharing](how-does-loading-work.md#Loading-native-code-with-RELRO-sharing) for
more details if you're curious how this process works.

You generally should not hit this issue unless you're trying to install a target
which is not WebView-capable (ex. `chrome_public_apk` instead of
`monochrome_public_apk`).

[aosp]: aosp-system-integration.md#Configuring-the-Android-framework
