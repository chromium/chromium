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

Add entries to the `excludes` list for the respective apk under `test_runs`.

## Re-enabling skipped tests

Before re-enabling tests, make sure it's actually safe to enable the test again.

* If the test was fixed on the Android side, the fix must be cherry-picked back
  to the earliest applicable version. Ex. if the test was added in Android Oreo
  (API 26), the fix should be picked back to `aosp/oreo-cts-dev`.
    * **Note:** some OS levels are no longer supported by the CTS team, and will
      no longer receive CTS releases. Unfortunately, if there was a test bug for
      these OS levels, we must disable the test forever on that OS (and you
      should cherry-pick the fix to the earliest supported CTS branch).
* If the failure was due to a chromium-side bug/regression, you can re-enable
  the test as soon as the bug is fixed on trunk.

Re-enabling the test case is as simple as removing it from the `excludes` for
the relevant OS levels. Please verify this change by adding the
`android-webview-*` trybots (not enabled by default).
