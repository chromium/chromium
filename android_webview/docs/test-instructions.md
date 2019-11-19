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
```

*** aside
You can optionally use `ClassName.methodName` instead of `ClassName#methodName`;
the chromium test runner understands either syntax.
***

### Java unittests

These tests live under `//android_webview/junit/` and use Robolectric.

```sh
# Build
$ autoninja -C out/Default android_webview_junit_tests

# Run tests (any of these commands):
$ out/Default/bin/run_android_webview_junit_tests # All tests
$ out/Default/bin/run_android_webview_junit_tests -f *FindAddressTest#* # Same glob patterns work here
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
```

### Layout tests and page cycler tests

WebView's layout tests and page cycler tests exercise the WebView installed on
the system, instrumenting the WebView shell (`system_webview_shell_apk`,
`org.chromium.webview_shell`). These test cases are defined in
`//android_webview/tools/system_webview_shell/`.

```sh
# Build
$ autoninja -C out/Default system_webview_shell_layout_test_apk

# Install the desired WebView APK
...

# Run layout tests (installs WebView shell):
$ out/Default/bin/run_system_webview_shell_layout_test_apk
```

To run page cycler tests instead, use the `system_webview_shell_page_cycler_apk`
target and test runner in the steps above.

### Useful test runner options

#### Debugging flaky tests

```sh
$ out/Default/bin/run_webview_instrumentation_test_apk \ # Any test runner
    --num_retries=0 \ # Tests normally retry-on-failure; disable for easier repo
    --repeat=1000 \ # Repeat up to 1000 times for a failure
    --break-on-failure \ # Stop repeating once we see the first failure
    -f=AwContentsTest#testClearCacheInQuickSuccession
```

#### Enable a Feature for a local run

```sh
$ out/Default/bin/run_webview_instrumentation_test_apk \ # Any test runner
    # Desired Features; see commandline-flags.md for more information
    --enable-features="MyFeature,MyOtherFeature" \
    -f=AwContentsTest#testClearCacheInQuickSuccession
```

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
```

To disable failing CTS tests, please see the cts_config
[README](../tools/cts_config/README.md) file.

If you'd like to edit these tests, see internal documentation at
http://go/clank-webview for working with Android checkouts.

### AndroidX (Support Library)

WebView also has an AndroidX module, which has its own tests (similar to CTS
tests). These tests live under the AOSP source tree, under
`//platform/frameworks/support/`.

TODO(ntfschr): document the solution for http://crbug.com/891102, when that's
fixed.
