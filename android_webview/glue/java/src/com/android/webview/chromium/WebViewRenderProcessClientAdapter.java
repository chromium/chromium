// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import android.annotation.TargetApi;
import android.os.Build;
import android.webkit.WebView;
import android.webkit.WebViewRenderProcess;
import android.webkit.WebViewRenderProcessClient;

import org.chromium.android_webview.AwRenderProcess;
import org.chromium.base.annotations.VerifiesOnQ;

import java.util.concurrent.Executor;

@VerifiesOnQ
@TargetApi(Build.VERSION_CODES.Q)
class WebViewRenderProcessClientAdapter extends SharedWebViewRendererClientAdapter {
    private Executor mExecutor;
    private WebViewRenderProcessClient mWebViewRenderProcessClient;

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