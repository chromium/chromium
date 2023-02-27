package org.chromium.sdk_sandbox.webview_sdk;

interface IWebViewSdkApi {
    oneway void loadUrl(String url);
    oneway void destroy();
}