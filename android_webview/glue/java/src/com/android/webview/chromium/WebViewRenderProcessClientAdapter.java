// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import android.webkit.WebView;
import android.webkit.WebViewRenderProcess;
import android.webkit.WebViewRenderProcessClient;

import org.chromium.android_webview.AwRenderProcess;
import org.chromium.android_webview.common.Lifetime;

import java.util.concurrent.Executor;

@Lifetime.WebView
class WebViewRenderProcessClientAdapter extends SharedWebViewRendererClientAdapter {
    private final Executor mExecutor;
    private final WebViewRenderProcessClient mWebViewRenderProcessClient;

    public WebViewRenderProcessClientAdapter(
            Executor executor, WebViewRenderProcessClient webViewRenderProcessClient) {
        mExecutor = executor;
        mWebViewRenderProcessClient = webViewRenderProcessClient;
    }

    public WebViewRenderProcessClient getWebViewRenderProcessClient() {
        return mWebViewRenderProcessClient;
    }

    @Override
    public void onRendererUnresponsive(final WebView view, final AwRenderProcess renderProcess) {
        WebViewRenderProcess renderer = WebViewRenderProcessAdapter.getInstanceFor(renderProcess);
        mExecutor.execute(
                () -> mWebViewRenderProcessClient.onRenderProcessUnresponsive(view, renderer));
    }

    @Override
    public void onRendererResponsive(final WebView view, final AwRenderProcess renderProcess) {
        WebViewRenderProcess renderer = WebViewRenderProcessAdapter.getInstanceFor(renderProcess);
        mExecutor.execute(
                () -> mWebViewRenderProcessClient.onRenderProcessResponsive(view, renderer));
    }
}
