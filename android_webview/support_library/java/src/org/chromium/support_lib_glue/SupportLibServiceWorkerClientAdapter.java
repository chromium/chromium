// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_glue;

import static org.chromium.support_lib_glue.SupportLibWebViewChromiumFactory.recordApiCall;

import android.webkit.WebResourceResponse;

import com.android.webview.chromium.WebResourceRequestAdapter;

import org.chromium.android_webview.AwContentsClient.AwWebResourceRequest;
import org.chromium.android_webview.AwServiceWorkerClient;
import org.chromium.base.TraceEvent;
import org.chromium.components.embedder_support.util.WebResourceResponseInfo;
import org.chromium.support_lib_boundary.ServiceWorkerClientBoundaryInterface;
import org.chromium.support_lib_boundary.util.BoundaryInterfaceReflectionUtil;
import org.chromium.support_lib_boundary.util.Features;
import org.chromium.support_lib_glue.SupportLibWebViewChromiumFactory.ApiCall;

/** Adapter between ServiceWorkerClientBoundaryInterface and AwServiceWorkerClient. */
class SupportLibServiceWorkerClientAdapter extends AwServiceWorkerClient {
    ServiceWorkerClientBoundaryInterface mImpl;

    SupportLibServiceWorkerClientAdapter(ServiceWorkerClientBoundaryInterface impl) {
        mImpl = impl;
    }

    @Override
    public WebResourceResponseInfo shouldInterceptRequest(AwWebResourceRequest request) {
        try (TraceEvent event =
                TraceEvent.scoped(
                        "WebView.APICall.AndroidX.SERVICE_WORKER_CLIENT_SHOULD_INTERCEPT_REQUEST")) {
            recordApiCall(ApiCall.SERVICE_WORKER_CLIENT_SHOULD_INTERCEPT_REQUEST);
            if (!BoundaryInterfaceReflectionUtil.containsFeature(
                    mImpl.getSupportedFeatures(),
                    Features.SERVICE_WORKER_SHOULD_INTERCEPT_REQUEST)) {
                // If the shouldInterceptRequest callback isn't supported, return null;
                return null;
            }
            WebResourceResponse response =
                    mImpl.shouldInterceptRequest(new WebResourceRequestAdapter(request));
            if (response == null) {
                return null;
            }
            return new WebResourceResponseInfo(
                    response.getMimeType(),
                    response.getEncoding(),
                    response.getData(),
                    response.getStatusCode(),
                    response.getReasonPhrase(),
                    response.getResponseHeaders());
        }
    }
}
