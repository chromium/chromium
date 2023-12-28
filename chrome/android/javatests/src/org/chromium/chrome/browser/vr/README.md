# XR Instrumentation Tests

## TL;DR For Most Local Repros

1. Get a rooted Pixel device of some sort.
2. Set lock screen timeout to at least 5 minutes. If screen is locked or device
   goes to sleep while tests are still running, they will fail.
3. Run `ninja -C out/Debug chrome_public_test_vr_apk
        && out/Debug/bin/run_chrome_public_test_vr_apk
        --num-retries=0
        --test-filter=<failing test case>`
   Don't touch phone while the tests are running.

If you are reproducing an issue with the AR tests, run
`export DOWNLOAD_XR_TEST_APKS=1 && gclient runhooks` in order to get the
playback datasets that are necessary. This requires authentication, run
`gsutil.py config` [documentation](https://chromium.googlesource.com/chromiumos/docs/+/main/gsutil.md)
to set this up if necessary.

**NOTE** The message "Main  Unable to find package info for org.chromium.chrome"
         is usually displayed when the test package is being installed and does
         not indicate any problem.

## Introduction

This directory contains all the Java-side infrastructure for running
instrumentation tests for [WebXR][webxr_spec]
(VR/Virtual Reality and AR/Augmented Reality) features currently in Chrome.

These tests are integration/end-to-end tests run in the full Chromium browser on
actual Android devices.

## Directories

These are the files and directories that are relevant to XR instrumentation
testing.

### Subdirectories

* `rules/` - Contains all the XR-specific JUnit4 rules for handling
functionality such as running tests multiple times in different activities and
handling the fake VR pose tracker service.
* `util/` - Contains utility classes with code that is used by multiple test
classes and that does not make sense to include in the core test framework.

### Other Directories

* [`//chrome/test/data/xr/e2e_test_files/`][html_dir] - Contains the JavaScript
and HTML files for XR instrumentation tests.
* [`//third_party/arcore-android-sdk/test-apks`][ar_test_apks] - Contains the AR
APKs used for testing, such as ArCore. You must have `DOWNLOAD_XR_TEST_APKS` set
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

**NOTE** Using this argument for pre-installed system apps will fail. This can
be dealt with in the following ways:

* Use `--replace-system-package path/to/apk/to/install`
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

## Adding New Tests

See [adding_new_tests.md][adding_new_tests].

[webxr_spec]: https://immersive-web.github.io/webxr-samples/explainer.html
[html_dir]: https://chromium.googlesource.com/chromium/src/+/main/chrome/test/data/xr/e2e_test_files
[ar_test_apks]: https://chromium.googlesource.com/chromium/src/+/main/third_party/arcore-android-sdk/test-apks
[adding_new_tests]:
https://chromium.googlesource.com/chromium/src/+/main/chrome/android/javatests/src/org/chromium/chrome/browser/vr/adding_new_tests.md
