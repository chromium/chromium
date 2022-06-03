# Android WebView CTS Test Configuration

Test apk(s) and tests to run on Android are configurable on a per
Android release basis by editing
[`webview_cts_gcs_path.json`](webview_cts_gcs_path.json).

## File format
```json
{
  {
    "<Android dessert letter>": {
      "arch": {
        "<arch1>": {
          "filename": "<relative path to cts_archive_dir of cts zip>",
          "_origin": "<CTS zip download url>",
          "unzip_dir": "<relative path to work directory where cts should be unzipped to>"
        },
        "<arch2>": {
          "filename": "<relative path to cts_archive_dir of cts zip>",
          "_origin": "<CTS zip download url>",
          "unzip_dir": "<relative path to work directory where cts should be unzipped to>"
        }
      },
      "test_runs": [
        {
          "apk": "location of the test apk in the cts zip file",
          "excludes": [
            {
              "match": "<class#testcase (wildcard supported) expression of test to skip>",
              "_bug_id": "<bug reference comment, optional>"
            }
          ]
        },
        {
          "apk": "location of the test apk in the cts zip file",
          "includes": [
            {
              "match": "<class#testcase (wildcard supported) expression of test to run>"
            }
          ]
        }
      ]
    }
  },
  ...
}
```

*** note
**Note:** Test names in the include/exclude list could change between releases,
please adjust them accordingly.
***

*** note
**Note:** If includes nor excludes are specified, all tests in the apk will run.
***

## Disabling/Skipping tests

**CTS regressions are more serious than most test failures.** CTS failures block
Android vendors from shipping devices and prevent the WebView team from dropping
new Chrome and WebView APKs in the Android source tree. If you need to disable a
test, please file a P1 crbug with **ReleaseBlock-Dev** in the `Mobile>WebView`
component.

If you must disable a test, you can add an entry to the `excludes` list for the
correct apk (most tests belong to `CtsWebkitTestCases.apk`) under `test_runs`
for each OS level which is failing.

## Re-enabling skipped tests

Before re-enabling tests, make sure it's actually safe to enable the test again.

* The test source code lives in Android and line numbers vary between OS
  versions. You can find test code for a particular CTS release by finding the
  appropriate git branch in codesearch:
    * Lollipop: [lollipop-mr1-cts-release]
    * Marshmallow: [marshmallow-cts-release]
    * Nougat: [nougat-cts-dev]
    * Oreo: [oreo-cts-dev]
    * Pie: [pie-cts-dev]
    * Android 10 (Q): [android10-tests-dev]
* If the test was fixed on the Android side, the fix must be cherry-picked back
  to the earliest applicable version (see the git branches above). Ex. if the
  test was added in Android Oreo (API 26), the fix should be picked back to
  `aosp/oreo-cts-dev`.
    * **Note:** some OS levels are no longer supported by the CTS team and will
      no longer receive CTS releases. Unfortunately, if there was a test bug for
      these OS levels, we must disable the test forever on that OS (and you
      should cherry-pick the fix to the earliest supported CTS branch).
* If the failure was due to a chromium-side bug/regression, you can re-enable
  the test as soon as the bug is fixed on trunk. You can run CTS on a device or
  emulator with [this guide](/android_webview/docs/test-instructions.md#cts).

Re-enabling the test case is as simple as removing it from the `excludes` for
the relevant OS levels. Please verify this change by adding the
`android-webview-*` trybots (not enabled by default).

## Changing CTS tests retroactively

Android generally has strict backward compatibility requirements, and this
extends to CTS. However, sometimes it's appropriate to change the test logic
rather than restoring the old chromium behavior, such as when the test logic is
responsible for flakiness or relies on an invalid assumption. Please reach out
to [the WebView team][1] quickly if you think a CTS test needs to change (the
failure is still considered **ReleaseBlock-Dev** until the test change actually
lands in Android).

Any CTS changes must be backward compatible. The original WebView version which
shipped on that OS version must continue to pass the revised CTS test.

[1]: https://groups.google.com/a/chromium.org/forum/#!forum/android-webview-dev
[lollipop-mr1-cts-release]: https://cs.android.com/android/platform/superproject/+/lollipop-mr1-cts-release:cts/tests/tests/webkit/src/android/webkit/cts/
[marshmallow-cts-release]: https://cs.android.com/android/platform/superproject/+/marshmallow-cts-release:cts/tests/tests/webkit/src/android/webkit/cts/
[nougat-cts-dev]: https://cs.android.com/android/platform/superproject/+/nougat-cts-dev:cts/tests/tests/webkit/src/android/webkit/cts/
[oreo-cts-dev]: https://cs.android.com/android/platform/superproject/+/oreo-cts-dev:cts/tests/tests/webkit/src/android/webkit/cts/
[pie-cts-dev]: https://cs.android.com/android/platform/superproject/+/pie-cts-dev:cts/tests/tests/webkit/src/android/webkit/cts/
[android10-tests-dev]: https://cs.android.com/android/platform/superproject/+/android10-tests-dev:cts/tests/tests/webkit/src/android/webkit/cts/
