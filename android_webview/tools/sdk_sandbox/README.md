# WebView SDK Sandbox Test App and SDK

The WebView SDK Sandbox Test App and SDK are a standalone application and sdk
for testing WebView in a privacy sandbox environment.

How to build and run:
->Ensure you are testing with a device/emulator that supports Privacy Sandbox
(https://developer.android.com/design-for-safety/privacy-sandbox/download#device)
->Add “target_os = "android"” to gn args
->Add “android_sdk_release = "tprivacysandbox"” to gn args
->Build “autoninja -C out/{testfolder} sdk_sandbox”
->Install Sdk first -> out/{testfolder}/bin/sdk_sandbox_webview_sdk install
->Install client app -> out/{testfolder}/bin/sdk_sandbox_webview_client install
