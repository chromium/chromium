# Net debugging in WebView

## Net log

WebView supports the `kLogNetLog` flag to log debugging network info to a JSON
file on disk.

### Please do not request netlogs from reporters

*** note
**Important**: at the moment, WebView netlog requires applying commandline
flags. **It's not typically possible for external reporters to apply commandline
flags, so please do not ask them to follow this guide.**

This guide is only for chromium developers who are set up for WebView
development. Specifically, this guide requires the reader to use a `userdebug`
or `eng` Android image, see [device setup](device-setup.md) and [commandline
flags](commandline-flags.md) for more information.
***

### Python script

If you have a chromium checkout, the preferred way to set the netlog flag is to
use the `record_netlog.py` script like so:

```shell
# Optional: set any flags of your choosing before running the script. Don't set
# --log-net-log though; this is set by record_netlog.py.
$ build/android/adb_system_webview_command_line --enable-features=MyFeature,MyOtherFeature
Wrote command line file. Current flags (in webview-command-line):
  005d1ac915b0c7d6 (bullhead-userdebug 6.0 MDB08M 2353240 dev-keys): --enable-features=MyFeature,MyOtherFeature

# Replace "<app package name>" with your app's package name (ex. the
# WebView Shell is "org.chromium.webview_shell"). This script will set an
# appropriate value for --log-net-log and handle setup/cleanup.
$ android_webview/tools/record_netlog.py --package="<app package name>"
Netlog will start recording as soon as app starts up. Press ctrl-C to stop recording.
^C
Pulling netlog to "netlog.json"
```

Then import the JSON file (`netlog.json` in the working directory) into [the
NetLog viewer][1].

### Manual steps

1. Figure out the app's data directory
   ```sh
   # appPackageName is the package name of whatever app you're interested (ex.
   # WebView shell is "org.chromium.webview_shell").
   appDataDir="$(adb shell dumpsys package ${appPackageName} | grep 'dataDir=' | sed 's/^ *dataDir=//')" && \
   ```
1. Pick a name for the JSON file. This must be under the WebView folder in the
   app's data directory (ex. `jsonFile="${appDataDir}/app_webview/foo.json"`).
   **Note:** it's important this is inside the data directory, otherwise
   multiple WebView apps might try (and succeed) to write to the file
   simultaneously.
1. Kill the app, if running
1. [Set the netlog flag](commandline-flags.md):
   ```sh
   FLAG_FILE=/data/local/tmp/webview-command-line
   adb shell "echo '_ --log-net-log=${jsonFile}' > ${FLAG_FILE}"
   ```
1. Restart the app. Reproduce whatever is of interest, and then kill the app
   when finished
1. Get the netlog off the device:
   ```sh
   adb pull "${appDataDir}/app_webview/${jsonFile}"
   adb shell "rm '${appDataDir}/app_webview/${jsonFile}'"
   ```
1. Optional: view the data in [the NetLog viewer][1]
1. Optional: [clear the commandline flags](commandline-flags.md):
   ```sh
   FLAG_FILE=/data/local/tmp/webview-command-line
   adb shell "rm ${FLAG_FILE}"
   ```

[1]: https://netlog-viewer.appspot.com/#import
