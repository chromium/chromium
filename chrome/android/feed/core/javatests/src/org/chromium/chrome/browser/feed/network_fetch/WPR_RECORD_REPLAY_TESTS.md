# WPR Record/Replay Tests

WPR Record/Replay tests are tests that utilize WPR to simulate a
real backend api service point. When executing a WPR test, there is a Chrome proxy
session being set up. Inside this Chrome proxy session, there are
webpagereplay server (aka WPR server), tsProxy server, android forwarder binary,
and the wiring of these component to each other. A system diagram is available
[here](https://docs.google.com/document/d/1xk2ZNGFSQZ8gjc5fCFSck4-WUQehU6GmetMlP-GXYRc/edit)

For a typical WPR Record/Replay test, there are two exeuction modes:
1. Record mode
2. Replay mode.

## Update your gclient config

You need to add the following lines to your .gclient checkout.

*  "checkout_src_internal": True,
*  "checkout_mobile_internal": True,
*  "checkout_wpr_archives": True,


Here is an example.

```
solutions = [
  {
    "url": "https://chromium.googlesource.com/chromium/src.git",
    "managed": False,
    "name": "src",
    "custom_deps": {},
    "custom_vars": {
      "checkout_src_internal": True,
      "checkout_mobile_internal": True,
      "checkout_wpr_archives": True,
    },
  },
]
target_os = ['android']
```

## Mark tests as WPR Record/Replay tests

To mark a test WPR Record/Replay test, there are two annotations to mark on the test method:
1. Features annotation should have 'WPRRecordReplayTest'
2. WPRArchiveDirectory that has a path points to the wpr archive folder, with
   name typically wpr_tests. It can not be a file.

Here is an example.

```
    @Test
    @MediumTest
    @Feature({"FeedNewTabPage", "WPRRecordReplayTest", "RenderTest"})
    @WPRArchiveConfigFilePath("chrome/android/feed/core/javatests/src/org/chromium/chrome/"
            + "browser/feed/network_fetch/test_data.json")
    public void
    launchNtp_withMultipleFeedCardsRendered() throws IOException, InterruptedException {
    ...
    }
```

## WPR Test file

The source path only contains some .sha1 files in the path. The wpr archive
files are stored in GCS bucket (gs://chrome-wpr-archives).

WPR test uses gclient runhooks to download wpr archives from GCS bucket
(gs://chrome-wpr-archives). It is using checkout_wpr_archives as a custom vars
 to make it ready to run on bot. If a bot wants to enable wpr_tests, add
 enable_wpr_tests config item in their gclient_apply_config setting.


## WPR tests running in record mode

In this mode, the WPR server forwards all the requests and responses between
the client and the backend server. Those requests and responses are also stored
locally as an archive file (typically ends with .wprgo). Those archive files are
stored in GCS bucket (gs://chrome-wpr-archives) with access control.

### Execute WPR test in record mode

To run the WPR test in record mode, a command line argument
'--wpr-enable-record' is needed.

A example command line:

```
out/X86/bin/run_chrome_java_test_wpr_tests --vv --wpr-enable-record
<test_filter>
```

After the test run, there should be a wprgo file, as untracked file, located at the same
directory of the path annotated with WPRArchiveDirectory. That wprgo
file should have the test's unique name as file name.

### Update WPR archive file via tools

WPR Record/Replay uses this
[script](chrome/test/data/android/manage_wpr_archives.py) to update WPR
archive files to the GCS (gs://chrome-wpr-archives).

The script requires 1 arguments

A example command line:

```
vpython3 chrome/test/data/android/manage_wpr_archives.py upload
```

The script also support a --dry_run command line flag.

## WPR tests running in replay mode

In this mode, the WPR server uses the wprgo archive file downloaded from GCS
bucket, matches it with test unique name, and plays back responses to the requests
based on key/value match.

This techniques makes the test to be hermatic. WPR tests running in reply mode is suitable
for running on CI/CQ bots.
