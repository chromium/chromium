// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import android.annotation.TargetApi;
import android.os.Build;
import android.webkit.WebResourceResponse;
import android.webkit.WebView;
import android.webkit.WebViewClient;

import org.chromium.android_webview.AwContentsClient;
import org.chromium.android_webview.AwWebResourceResponse;
import org.chromium.base.annotations.VerifiesOnM;

/**
 * Utility class to use new APIs that were added in M (API level 23). These need to exist in a
 * separate class so that Android framework can successfully verify glue layer classes without
 * encountering the new APIs. Note that GlueApiHelper is only for APIs that cannot go to ApiHelper
 * in base/, for reasons such as using system APIs or instantiating an adapter class that is
 * specific to glue layer.
 */
@VerifiesOnM
@TargetApi(Build.VERSION_CODES.M)
public final class GlueApiHelperForM {
    private GlueApiHelperForM() {}

    /**
     * See {@link WebViewClient#onReceivedError(WebView, WebResourceRequest, WebResourceError)},
     * which was added in M.
     */
    public static void onReceivedError(WebViewClient webViewClient, WebView webView,
            AwContentsClient.AwWebResourceRequest request,
            AwContentsClient.AwWebResourceError error) {
        webViewClient.onReceivedError(webView, new WebResourceRequestAdapter(request),
                new WebResourceErrorAdapter(error));
    }

    /**
     * See {@link WebViewClient#onReceivedHttpError(WebView, WebResourceRequest,
     * WebResourceResponse)}, which was added in M.
     *
     * Note that creation of WebResourceResponse with 'immutable' parameter is non-public.
     */
    public static void onReceivedHttpError(WebViewClient webViewClient, WebView webView,
            AwContentsClient.AwWebResourceRequest request, AwWebResourceResponse response) {
        webViewClient.onReceivedHttpError(webView, new WebResourceRequestAdapter(request),
                new WebResourceResponse(true, response.getMimeType(), response.getCharset(),
                        response.getStatusCode(), response.getReasonPhrase(),
                        response.getResponseHeaders(), response.getData()));
    }
}
