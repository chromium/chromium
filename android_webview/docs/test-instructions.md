# WebView Test Instructions

[TOC]

## Android Instructions

Please follow the instructions at
[android_test_instructions.md](/docs/testing/android_test_instructions.md).
This guide is an extension with WebView-specific content.

*** note
**Note:** except where otherwise noted, all tests require a device or emulator.
***

## Chromium-side tests

### Instrumentation tests

These tests live under `//android_webview/javatests/`, and are mostly
end-to-end (*with the exception of the `//android_webview/glue/` layer*).

```sh
# Build
$ autoninja -C out/Default webview_instrumentation_test_apk

# Run tests (any of these commands):
$ out/Default/bin/run_webview_instrumentation_test_apk # All tests
$ out/Default/bin/run_webview_instrumentation_test_apk -f AwContentsTest#* # A particular test suite
$ out/Default/bin/run_webview_instrumentation_test_apk -f AwContentsTest#testClearCacheInQuickSuccession # A single test
$ out/Default/bin/run_webview_instrumentation_test_apk -f AwContentsTest#*Succession # Any glob pattern matching 1 or more tests

# Print both Java and C++ log messages to the console (optional):
$ adb logcat
```

*** aside
You can optionally use `ClassName.methodName` instead of `ClassName#methodName`;
the chromium test runner understands either syntax.
***

### Java unittests

These tests live under `//android_webview/junit/` and use Robolectric.

*** promo
**Tip:** Robolectric tests run on workstation and do not need a device or
emulator. These generally run much faster than on-device tests.
***

```sh
# Build
$ autoninja -C out/Default android_webview_junit_tests

# Run tests (any of these commands):
$ out/Default/bin/run_android_webview_junit_tests # All tests
$ out/Default/bin/run_android_webview_junit_tests -f *FindAddressTest#* # Same glob patterns work here

# Print both Java and C++ log messages to the console (optional) by passing "-v"
# to the test runner. Example:
$ out/Default/bin/run_android_webview_unittests -v # All tests, including logs
```

*** note
For junit tests, filter (`-f`) arguments require fully qualified class names
(e.g. `org.chromium.android_webview.robolectric.FindAddressTest`), but replacing
the package name with a glob wildcard (`*`), as in the example above, will work
if the class name is unique.
***

### Native unittests

These are any `*_test.cc` or `*_unittest.cc` test under `//android_webview/`.
Currently, we only have tests under `//android_webview/browser/` and
`//android_webview/lib/`.

```sh
# Build
$ autoninja -C out/Default android_webview_unittests

# Run tests (any of these commands):
$ out/Default/bin/run_android_webview_unittests # All tests
$ out/Default/bin/run_android_webview_unittests -f AndroidStreamReaderURLRequestJobTest.* # Same glob patterns work here

# Print both Java and C++ log messages to the console (optional):
$ adb logcat
```

### Layout tests and page cycler tests

WebView's layout tests and page cycler tests exercise the **WebView installed on
the system** and instrument the [system WebView shell app](webview-shell.md)
(`system_webview_shell_apk`). These test cases are defined in
`//android_webview/tools/system_webview_shell/`.

*** note
**Important:** these tests compile and install both `system_webview_apk` and
`system_webview_shell_apk`.

You will need to configure GN args to make sure `system_webview_apk` is a valid
WebView provider for your system. Please see the [full build
instructions](build-instructions.md).

If you are using an **emulator**, you will also need to configure the
`system_webview_shell_package_name` GN arg. See [WebView shell
docs](webview-shell.md#setting-up-the-build) for details.
***

```sh
# Build (this also compiles system_webview_shell_apk and system_webview_apk)
$ autoninja -C out/Default system_webview_shell_layout_test_apk

# Run layout tests (installs the test APK, WebView shell, and
# system_webview_apk, and also switches your WebView provider)
$ out/Default/bin/run_system_webview_shell_layout_test_apk

# Print both Java and C++ log messages to the console (optional):
$ adb logcat
```

To run page cycler tests instead, use the `system_webview_shell_page_cycler_apk`
target and test runner in the steps above.

### UI tests

Like [layout and page cycler tests](#Layout-tests-and-page-cycler-tests),
WebView UI tests use the WebView installed on the system (and will automatically
compile and install the `system_webview_apk` target). Unlike those tests
however, this test suite _does not_ depend on the system WebView shell app, so
the setup is simpler. You will still need to follow the [full build
instructions](build-instructions.md) to correctly configure the
`system_webview_apk` target, but will not need to worry about compiling the
WebView shell (and do not need to worry about https://crbug.com/1205665).

```sh
# Build (this also compiles system_webview_apk)
$ autoninja -C out/Default webview_ui_test_app_test_apk

# Run layout tests (installs the test APK and system_webview_apk and also
# switches your WebView provider)
$ out/Default/bin/run_webview_ui_test_app_test_apk

# Print both Java and C++ log messages to the console (optional):
$ adb logcat
```

### Useful test runner options

#### Debugging flaky tests

```sh
$ out/Default/bin/run_webview_instrumentation_test_apk \ # Any test runner
    --num_retries=0 \ # Tests normally retry-on-failure; disable for easier repo
    --repeat=100 \ # Repeat up to 100 times for a failure
    --break-on-failure \ # Stop repeating once we see the first failure
    -f=AwContentsTest#testClearCacheInQuickSuccession
```

A bash for loop can be used instead if the flake seems to happen during
specific conditions that need to be configured before each test run:

```sh
$ for (( c=1; c<100; c++ ))
$ do
$   echo "\n\n\nTest $c/100 \n\n\n"
$   <Any setup command you need to do - eg: adb reboot>
$   out/Default/bin/run_webview_instrumentation_test_apk \ # Any test runner
        --num_retries=0 \ # Tests normally retry-on-failure; disable for easier repo
        -f=
$ done
```

#### Enable a Feature for a local run

```sh
$ out/Default/bin/run_webview_instrumentation_test_apk \ # Any test runner
    # Desired Features; see commandline-flags.md for more information
    --enable-features="MyFeature,MyOtherFeature" \
    -f=AwContentsTest#testClearCacheInQuickSuccession
```

#### Debugging hangs in instrumentation tests

If an instrumentation test is hanging, it's possible to get a callstack from the
browser process. This requires running on a device with root.

It's not possible to get a callstack from the renderer because the sandbox will
prevent the trace file from being written. A workaround if you want to see the
renderer threads is to run in single-process mode by adding
`@OnlyRunIn(SINGLE_PROCESS)` above the test.

##### conventions

| Label     |                                                                |
|-----------|----------------------------------------------------------------|
|  (shell)  | in your workstation's shell                                    |
|  (phone)  | inside the phone's shell which you entered through `adb shell` |

```sh
# Find the pid
$ (shell) adb root
$ (shell) adb shell

# Get the main WebView Shell pid, e.g. org.chromium.android_webview.shell and
# not org.chromium.android_webview.shell:sandboxed_process0
$ (phone) ps -A | grep org.chromium.android_webview.shell
# Generate a callstack (this won't kill the process)
$ (phone) kill -3 pid
# Look for the latest trace
$ (phone) ls /data/anr/ -l
# Copy the trace locally
$ (shell) adb pull /data/anr/trace_01 /tmp/t1
# Generate a callstack. Run this from the source directory.
$ (shell) third_party/android_platform/development/scripts/stack --output-directory=out/Release /tmp/t1
```

#### Instrumentation test process modes

You may use `--webview-process-mode` argument to run tests only in the
specified process mode. The valid switch values are `multiple` and `single`.
When the argument is not set (default), both process modes execute. Note that
the argument acts as an additional filter on top of the
[`OnlyRunIn` test annotation](https://source.chromium.org/chromium/chromium/src/+/main:android_webview/test/shell/src/org/chromium/android_webview/test/OnlyRunIn.java;drc=02c4c92d88aecbc14e715fd7fcac842d5dd814fe;l=25).

Also note that in chromium CQ builders and all builders running tests on
Android R and plus, `--webview-process-mode=multiple` is appended to the test
command so that the instrumentation tests only run in multiple processes mode.

## External tests

As WebView is an Android system component, we have some tests defined outside of
the chromium repository, but which the team still maintains. For some of these
tests, we have scripts to help chromium developers check these tests.

*** promo
All of these tests are end-to-end, so they exercise whatever WebView
implementation you've installed and selected on your device. This also means you
can enable Features and commandline flags the same way [as you would for
production](./commandline-flags.md).
***

### CTS

WebView has [CTS](https://source.android.com/compatibility/cts) tests, testing
end-to-end behavior (using the WebView installed on the system). These tests
live in the Android source tree (under `//platform/cts/tests/tests/webkit/`).

Chromium developers can download and run pre-built APKs for these test cases
with:

```sh
# Install the desired WebView APK
...

# Run pre-built WebView CTS tests:
$ android_webview/tools/run_cts.py \
    --verbose \ # Optional
    -f=android.webkit.cts.WebViewTest#* # Supports similar test filters

# Print both Java and C++ log messages to the console (optional):
$ adb logcat
```

*** promo
**Tip:** make sure your device locale is **English (United States)** per
[CTS setup requirements](https://source.android.com/compatibility/cts/setup).
***

To disable failing CTS tests, please see the cts_config
[README](../tools/cts_config/README.md) file.

If you'd like to edit these tests, see internal documentation at
http://go/clank-webview for working with Android checkouts.

### AndroidX (Support Library)

WebView also has an AndroidX module, which has its own tests (similar to CTS
tests). These tests live under the AOSP source tree, under
`//platform/frameworks/support/`.

TODO(ntfschr): document the solution for https://crbug.com/891102, when that's
fixed.
