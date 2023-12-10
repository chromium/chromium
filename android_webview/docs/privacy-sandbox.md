# Privacy Sandbox


[TOC]


The WebView team supports both the [Android](https://developer.android.com/design-for-safety/privacy-sandbox) and [Chromium](https://developer.chrome.com/en/docs/privacy-sandbox/) Privacy Sandbox
initiatives. Below please find details of work related to these efforts:


## SDK Runtime

The WebView SDK Sandbox Test App and SDK are a standalone application and sdk [in chromium](/android_webview/tools/sdk_sandbox/)
for testing WebView in a privacy sandbox environment.


*** note
This relies on the WebView installed on the system. So if you're trying to
verify local changes to WebView, or run against a specific WebView build, you
must **install WebView first.**
***


*** note
This is *not* a production quality browser and does not implement suitable
security UI to be used for anything other than testing WebView. This should not
be shipped anywhere or used as the basis for implementing a browser.
***


### Setting up the build

Ensure you are testing with a device/emulator that supports Privacy Sandbox
(https://developer.android.com/design-for-safety/privacy-sandbox/download#device).
You will need to add the following to your GN args:
```
target_os = "android"
```


### Building the app and sdk

```sh
$ autoninja -C out/Default sdk_sandbox
```

### Installing the sdk (required before installing the app)

```sh
$ out/Default/bin/sdk_sandbox_webview_sdk install
```

### Installing the app

```sh
$ out/Default/bin/sdk_sandbox_webview_client install
```

### Running the shell

Open the app via its launch icon in the menu. 

You will first need to click the "Load SDK" button to load the SDK that will return the WebView. 
Next click the "Load Surface Package" button to load the WebView in the space below.
The WebView will default to loading the Google home page, you can use the url bar to navigate to your desired test page.
You can also test unloading the SDK by clicking the "Unload SDK" button. In this case you will see the WebView killed.
