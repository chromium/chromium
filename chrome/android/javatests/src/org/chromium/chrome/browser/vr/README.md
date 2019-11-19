# XR Instrumentation Tests

## TL;DR For Most Local Repros

1. Get a rooted Pixel device of some sort.
2. Make sure "VR Services" is up to date in the Playstore.
3. Set lock screen timeout to at least 5 minutes. If screen is locked or device
   goes to sleep while tests are still running, they will fail.
4. Run `ninja -C out/Debug chrome_public_test_vr_apk
        && out/Debug/bin/run_chrome_public_test_vr_apk
        --num-retries=0
        --shared-prefs-file=//chrome/android/shared_preference_files/test/vr_ddview_skipdon_setupcomplete.json
        --test-filter=<failing test case>`
   Don't touch phone while the tests are running.

**NOTE** The message "Main  Unable to find package info for org.chromium.chrome"
         is usually displayed when the test package is being installed and does
         not indicate any problem.

## Introduction

This directory contains all the Java-side infrastructure for running
instrumentation tests for XR (VR/Virtual Reality and AR/Augmented Reality)
features currently in Chrome:

* [WebVR](https://webvr.info/) - Experience VR content on the web
* [WebXR](https://immersive-web.github.io/webxr-samples/explainer.html) -
Successor to WebVR, experience VR and AR content on the web
* VR Browser - Browse the 2D web from a VR headset

These tests are integration/end-to-end tests run in the full Chromium browser on
actual Android devices.

## Directories

These are the files and directories that are relevant to XR instrumentation
testing.

### Subdirectories

* `mock/` - Contains all the classes for mock implementations of XR classes.
* `nfc_apk/` - Contains the code for the standalone APK for NFC simulation. Used
by Telemetry tests, not instrumentation tests, but kept here since it uses code
from `util/`.
* `rules/` - Contains all the XR-specific JUnit4 rules for handling
functionality such as running tests multiple times in different activities and
handling the fake VR pose tracker service.
* `util/` - Contains utility classes with code that is used by multiple test
classes and that does not make sense to include in the core test framework.

### Other Directories

* [`//chrome/android/shared_preferences_files/test`][shared_prefs_dir] -
Contains the VrCore settings files for running VR instrumentation tests (see the
"Building and Running" section for more information).
* [`//chrome/test/data/xr/e2e_test_files/`][html_dir] - Contains the JavaScript
and HTML files for XR instrumentation tests.
* [`//third_party/gvr-android-sdk/test-apks`][vr_test_apks] - Contains the VR
APKs used for testing, such as VrCore. You must have `DOWNLOAD_VR_TEST_APKS` set
as an environment variable when you run gclient runhooks in order to actually
download these from storage.
* [`//third_party/gvr-android-sdk/test-libraries`][vr_test_libraries] - Contains
third party VR testing libraries. Currently, only has the Daydream controller
test library used for sending controller events to VrCore using intents.
* [`//third_party/arcore-android-sdk/test-apks`][ar_test_apks] - Contains the AR
APKs used for testing, such as ArCore. You must have `DOWNLOAD_VR_TEST_APKS` set
as an environment variable when you run gclient runhooks in order to actually
download these from storage.

## Building

### AR

The AR instrumentation tests can be built with the
`monochrome_public_test_ar_apk` target, which will also build
`monochrome_public_apk` to test with.

### VR

The VR instrumentation tests can be built with the `chrome_public_test_vr_apk`
target, which will also build `chrome_public_apk` to test with.

## Running

Both the VR and AR tests are run using the generated script in your build output
directory's `bin/` directory, e.g. `out/foo/bin/run_chrome_public_test_vr_apk`
to run the VR tests. You will likely need to pass some or all of the following
arguments in order for the tests to run properly, though.

**NOTE** The instrumentation tests can only be run on rooted devices.

### Common Arguments

#### additional-apk

`--additional-apk path/to/apk/to/install`

Installs the specified APK before running the tests. No-ops if the provided APK
is already installed on the device and their versions match.

For VR tests, you'll likely want to use `--additional-apk
third_party/gvr-android-sdk/test-apks/vr_services/vr_services_current.apk` to
ensure that the VrCore version used is the one used for automated testing at
whatever your current git revision is.

For AR tests, you'll likely want to use `--additional-apk
third_party/arcore-android-sdk/test-apks/arcore/arcore_current.apk` to ensure
that the ArCore version used is the one used for automated testing at whatever
your current git revision is.

**NOTE** Using this argument for VR on most Pixel devices will fail, as VrCore
is pre-installed as a system app. This can be dealt with in the following ways:

* Remove VrCore as a system app by following the instructions
  [here](go/vrcore/building-and-running). This will permanently solve the issue
  unless you reflash your device.
* Use `--replace-system-package
  com.google.vr.vrcore,//third_party/gvr-android-sdk/test-apks/vr_services/vr_services_current.apk`
  instead. This will take significantly longer, as it requires rebooting, and
  must be done every time you run the tests.
* Skip this argument entirely and just ensure that the VrCore version on the
  device is up to date via the Play Store.

#### test-filter

`--test-filter TestClass#TestCase`

Allows you to limit the set of tests run to a particular test class or subset of
tests within a test class. Use of the `*` wildcard is supported, e.g.
`--test-filter VrBrowserTransitionTest#*` will run all tests in the
VrBrowserTransitionTest class.

#### local-output/json-results-file

`--local-output --json-results-file output.json`

Sets the test runner to generate a local results summary after running all tests
and print out a file URL pointing to the summary. This allows you to view both
logcat output for a particular test and its post-failure screenshot.

#### num-retries

`--num-retries <#>`

Sets the test runner to retry failed tests a certain number of times. The
default is 2, resulting in a max of 3 test runs. Usually used as `--num-retries
0` when debugging to reduce test runtime and make flakiness more visible.

#### repeat

`--repeat <#>`

Sets the test runner to repeat the tests a certain number of times. The default
is 0, resulting in only one iteration. Usually used to repeat a test many times
in order to check for or reproduce flakiness.

### VR-Specific Arguments

#### shared-prefs-file

`--shared-prefs-file path/to/preference/json/file`

Configures VrCore according to the provided file, e.g. changing the paired
headset. The currently supported files are:

* `//chrome/android/shared_preference_files/test/vr_cardboard_skipdon_setupcomplete.json`
  will cause all cardboard-compatible tests to run. This will pair the device
  with a Cardboard headset and disable controller emulation.
* `//chrome/android/shared_preference_files/test/vr_ddview_skipdon_setupcomplete.json`
  will cause most Daydream View-compatible tests to run, with the exception of
  those that require the DON flow to be enabled. This will pair the device with
  a Daydream View headset, set the DON flow to be skipped, and enable controller
  emulation.
* `//chrome/android/shared_preference_files/test/vr_enable_vr_settings_service.json`
  combined with the extra `--vr-settings-service-enabled` and
  `--annotation=Restriction=VR_Settings_Service` flags will cause all tests that
  are using the `RESTRICTION_TYPE_VR_SETTINGS_SERVICE` restriction to run. See
  the section below for more detail on this.

The test runner will automatically revert any changed settings back to their
pre-test values after the test suite has completed. If for whatever reason you
want to manually apply settings outside of a test, you can do so with
`//build/android/apply_shared_preference_file.py`.

#### vr-settings-service-enabled

`--vr-settings-service-enabled --annotation=Restriction=VR_Settings_Service`

Tells the test runner to allow the running of tests that utilize the VR settings
service to dynamically change VrCore settings during a test instead of relying
on whatever was set by the shared preference file that was applied. This is used
as a catch-all for less standard tests, such as those that require the DON flow
to be enabled or that need to switch the paired viewer mid-test.

This should only be used when `--shared-prefs-file` is passed
`//chrome/android/shared_preference_files/test/vr_enable_vr_settings_service.json`
as otherwise trying to use the service will be a NOOP.

## Adding New Tests

See [adding_new_tests.md][adding_new_tests].


[shared_prefs_dir]:
https://chromium.googlesource.com/chromium/src/+/master/chrome/android/shared_preference_files/test
[html_dir]: https://chromium.googlesource.com/chromium/src/+/master/chrome/test/data/xr/e2e_test_files
[vr_test_apks]: https://chromium.googlesource.com/chromium/src/+/master/third_party/gvr-android-sdk/test-apks
[vr_test_libraries]: https://chromium.googlesource.com/chromium/src/+/master/third_party/gvr-android-sdk/test-libraries
[ar_test_apks]: https://chromium.googlesource.com/chromium/src/+/master/third_party/arcore-android-sdk/test-apks
[adding_new_tests]:
https://chromium.googlesource.com/chromium/src/+/master/chrome/android/javatests/src/org/chromium/chrome/browser/vr/adding_new_tests.md