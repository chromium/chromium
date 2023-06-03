// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import android.os.Build;
import android.webkit.CookieManager;
import android.webkit.SafeBrowsingResponse;
import android.webkit.ServiceWorkerWebSettings;
import android.webkit.WebMessagePort;
import android.webkit.WebResourceError;
import android.webkit.WebResourceRequest;
import android.webkit.WebSettings;
import android.webkit.WebView;

import androidx.annotation.RequiresApi;

import org.chromium.android_webview.AwContentsClient.AwWebResourceError;
import org.chromium.android_webview.AwContentsClient.AwWebResourceRequest;
import org.chromium.android_webview.AwCookieManager;
import org.chromium.android_webview.AwServiceWorkerSettings;
import org.chromium.android_webview.AwSettings;
import org.chromium.android_webview.common.Lifetime;
import org.chromium.android_webview.safe_browsing.AwSafeBrowsingResponse;
import org.chromium.base.Callback;
import org.chromium.content_public.browser.MessagePort;

/**
 * Class converting webkit objects to glue-objects shared between the webkit-glue and the support
 * library glue.
 * This class is used to minimize dependencies from the support-library-glue on the webkit-glue.
 */
@Lifetime.Singleton
public class WebkitToSharedGlueConverter {
    public static AwCookieManager getCookieManager(CookieManager cookieManager) {
        return ((CookieManagerAdapter) cookieManager).getCookieManager();
    }

    public static SharedWebViewChromium getSharedWebViewChromium(WebView webview) {
        WebViewChromium webviewChromium = (WebViewChromium) webview.getWebViewProvider();
        return webviewChromium.getSharedWebViewChromium();
    }

    public static AwSettings getSettings(WebSettings webSettings) {
        ContentSettingsAdapter contentSettingsAdapter = (ContentSettingsAdapter) webSettings;
        return contentSettingsAdapter.getAwSettings();
    }

    public static WebViewChromiumAwInit getGlobalAwInit() {
        return WebViewChromiumFactoryProvider.getSingleton().getAwInit();
    }

    public static AwServiceWorkerSettings getServiceWorkerSettings(
            ServiceWorkerWebSettings settings) {
        ServiceWorkerSettingsAdapter adapter = (ServiceWorkerSettingsAdapter) settings;
        return adapter.getAwSettings();
    }

    public static AwWebResourceRequest getWebResourceRequest(WebResourceRequest request) {
        WebResourceRequestAdapter adapter = (WebResourceRequestAdapter) request;
        return adapter.getAwResourceRequest();
    }

    public static AwWebResourceError getAwWebResourceError(WebResourceError error) {
        return ((WebResourceErrorAdapter) error).getAwWebResourceError();
    }

    @RequiresApi(Build.VERSION_CODES.O_MR1)
    public static Callback<AwSafeBrowsingResponse> getAwSafeBrowsingResponseCallback(
            SafeBrowsingResponse response) {
        return ((SafeBrowsingResponseAdapter) response).getAwSafeBrowsingResponseCallback();
    }

    public static MessagePort getMessagePort(WebMessagePort messagePort) {
        return ((WebMessagePortAdapter) messagePort).getPort();
    }
}
