// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.sdk_sandbox.webview_sdk;

import android.app.sdksandbox.SandboxedSdk;
import android.app.sdksandbox.SandboxedSdkProvider;
import android.content.Context;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.View;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/**
 * Implementation class for an sdk to test WebView that can be loaded by the sdk sandbox Sdk
 * provides a way to create/destroy a WebView and modify the url loaded in the WebView
 */
public class WebViewSandboxedSdkProvider extends SandboxedSdkProvider {
    private WebView mWebView;
    private static final Handler sHandler = new Handler(Looper.getMainLooper());

    @Override
    public SandboxedSdk onLoadSdk(Bundle params) {
        IWebViewSdkApi.Stub webviewProxy =
                new IWebViewSdkApi.Stub() {
                    @Override
                    public void loadUrl(String url) {
                        if (mWebView != null) {
                            sHandler.post(() -> mWebView.loadUrl(url));
                        }
                    }

                    @Override
                    public void destroy() {
                        if (mWebView != null) {
                            sHandler.post(() -> mWebView.destroy());
                        }
                    }
                };
        return new SandboxedSdk(webviewProxy);
    }

    @Override
    public View getView(Context windowContext, Bundle params, int width, int height) {
        final CountDownLatch latch = new CountDownLatch(1);
        try {
            generate(windowContext, latch);
            latch.await(2, TimeUnit.SECONDS);
            return mWebView;
        } catch (Exception e) {
            return null;
        }
    }

    private void generate(Context context, CountDownLatch latch) {
        mWebView = new WebView(context);
        WebSettings settings = mWebView.getSettings();
        initializeSettings(settings);

        mWebView.setWebViewClient(new WebViewClient());

        mWebView.loadUrl("https://www.google.com");
        latch.countDown();
    }

    private void initializeSettings(WebSettings settings) {
        settings.setJavaScriptEnabled(true);

        settings.setGeolocationEnabled(true);
        settings.setSupportZoom(true);
        settings.setDatabaseEnabled(true);
        settings.setDomStorageEnabled(true);
        settings.setAllowFileAccess(true);
        settings.setAllowContentAccess(true);

        // Default layout behavior for chrome on android.
        settings.setUseWideViewPort(true);
        settings.setLoadWithOverviewMode(true);
        settings.setLayoutAlgorithm(WebSettings.LayoutAlgorithm.TEXT_AUTOSIZING);
    }
}
