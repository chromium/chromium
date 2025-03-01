// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_glue;

import static org.chromium.support_lib_glue.SupportLibWebViewChromiumFactory.recordApiCall;

import android.webkit.WebResourceResponse;

import org.chromium.android_webview.WebResponseCallback;
import org.chromium.base.TraceEvent;
import org.chromium.components.embedder_support.util.WebResourceResponseInfo;
import org.chromium.support_lib_boundary.AsyncShouldInterceptRequestCallbackBoundaryInterface.WebResponseCallbackBoundaryInterface;
import org.chromium.support_lib_glue.SupportLibWebViewChromiumFactory.ApiCall;

/** Adapter between WebResponseCallbackBoundaryInterface and WebResponseCallback. */
class WebResponseCallbackAdapter implements WebResponseCallbackBoundaryInterface {
    private WebResponseCallback mImpl;

    public WebResponseCallbackAdapter(WebResponseCallback callback) {
        mImpl = callback;
    }

    @Override
    public void intercept(WebResourceResponse response) {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.AndroidX.WEB_RESPONSE_CALLBACK_INTERCEPT")) {
            recordApiCall(ApiCall.WEB_RESPONSE_CALLBACK_INTERCEPT);
            mImpl.intercept(fromWebResourceResponse(response));
        }
    }

    @Override
    public void doNotIntercept() {
        try (TraceEvent event =
                TraceEvent.scoped(
                        "WebView.APICall.AndroidX.WEB_RESPONSE_CALLBACK_DO_NOT_INTERCEPT")) {
            recordApiCall(ApiCall.WEB_RESPONSE_CALLBACK_DO_NOT_INTERCEPT);
            mImpl.intercept(null);
        }
    }

    private WebResourceResponseInfo fromWebResourceResponse(WebResourceResponse response) {
        return new WebResourceResponseInfo(
                response.getMimeType(),
                response.getEncoding(),
                response.getData(),
                response.getStatusCode(),
                response.getReasonPhrase(),
                response.getResponseHeaders());
    }
}
