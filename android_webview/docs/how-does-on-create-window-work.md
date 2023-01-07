# How does [`WebChromeClient#onCreateWindow`](https://developer.android.com/reference/android/webkit/WebChromeClient#onCreateWindow(android.webkit.WebView,%20boolean,%20boolean,%20android.os.Message)) work?

[TOC]

## Summary

This is a technical explanation of how `onCreateWindow` and the related API are
implemented from content layer APIs.

## Example usage

Let's look at example code snippets first to see how an app could use these API:

On the app side (in Java):

```java
// Configure parent WebView.
WebView webView = ...;
webView.getSettings().setJavaScriptEnabled(true);
webView.getSettings().setJavaScriptCanOpenWindowsAutomatically(true);
webView.getSettings().setSupportMultipleWindows(true);

webView.setWebChromeClient(new WebChromeClient() {
    @Override
    public boolean onCreateWindow(
            WebView view, boolean isDialog, boolean isUserGesture, Message resultMsg) {
        // Create child WebView. It is better to not reuse an existing WebView.
        WebView childWebView = ...;

        WebView.WebViewTransport transport = (WebView.WebViewTransport) resultMsg.obj;
        transport.setWebView(childWebView);
        resultMsg.sentToTarget();
        return true;
    }
});

webView.loadUrl(...);
```

On the web page side (in JavaScript):

```javascript
window.open("www.example.com");
```

## What happened under the hood

1. When the parent WebView loads the web page and runs the JavaScript snippet,
   [`AwWebContentsDelegate::AddNewContents`](https://source.chromium.org/chromium/chromium/src/+/main:android_webview/browser/aw_web_contents_delegate.h;l=43;drc=3abb32da2944ffe178dd66f404e7e1bb88a58ed0)
   will be called. The corresponding Java side
   [`AwWebContentsDelegate#addNewContents`](https://source.chromium.org/chromium/chromium/src/+/main:android_webview/java/src/org/chromium/android_webview/AwWebContentsDelegate.java;l=30;drc=a19051603849d7810b3569daf158aceb23aad1da)
   is called from the native.

1. At the same time,
   [`AwContents::SetPendingWebContentsForPopup`](https://source.chromium.org/chromium/chromium/src/+/main:android_webview/browser/aw_contents.cc;l=1099;drc=7776bbb38c4e394b5be085bc8c5bc02df5fa22dc)
   creates native popup AwContents with the given `WebContents` and stores it as
   `pending_contents_` in the parent `AwContents` object without Java
   counterpart created. Note that since `pending_contents_` can only store one
   popup AwContents, WebView doesn't support multiple pending popups.

1. `WebChromeClient#onCreateWindow` is called from step 1, with the code snippet
   above, `childWebView` is set to the `WebViewTransport` and
   `resultMsg.sendToTarget()` will send the `childWebView` to its receiver.

1. `WebViewContentsClientAdapter` has a handler that receives the message sent
   from `resultMsg.sendToTarget()`. It will trigger
   [`WebViewChromium#completeWindowCreation`](https://source.chromium.org/chromium/chromium/src/+/main:android_webview/glue/java/src/com/android/webview/chromium/WebViewChromium.java;l=265;drc=da3bb54157d4603b9c820d6cfdf61859f804dfb2),
   then
   [`AwContents#supplyContentsForPopup`](https://source.chromium.org/chromium/chromium/src/+/main:android_webview/java/src/org/chromium/android_webview/AwContents.java;l=1455;drc=4afe92995db1279895f8a40b69c374bc298d750f)
   is called on the parent WebView/AwContents.

1. `AwContents#supplyContentsForPopup` calls
   [`AwContents#receivePopupContents`](https://source.chromium.org/chromium/chromium/src/+/main:android_webview/java/src/org/chromium/android_webview/AwContents.java;l=1475;drc=4afe92995db1279895f8a40b69c374bc298d750f)
   on the child WebView/AwContents. Child AwContents deletes the existing native
   AwContents from the child WebView/AwContents, and pairs it with the
   `pending_contents_` from the parent WebView/AwContents. In order to preserve
   the status of the child WebView, all the flags and configurations need to be
   re-applied to the `pending_contents_`. Loading on the native AwContents is
   also resumed.
