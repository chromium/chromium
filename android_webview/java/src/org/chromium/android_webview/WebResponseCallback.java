// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import org.jni_zero.JNINamespace;

import org.chromium.components.embedder_support.util.WebResourceResponseInfo;

@JNINamespace("android_webview")
public final class WebResponseCallback {
    private static final String TAG = "WebRspnsCllbck";
    private final int mRequestId;
    private AwContentsClient mAwContentsClient;

    public WebResponseCallback(int requestId) {
        mRequestId = requestId;
    }

    public void intercept(
            WebResourceResponseInfo response, AwContentsClient.AwWebResourceRequest request) {
        if (mAwContentsClient != null) {
            if (response == null) {
                mAwContentsClient.getCallbackHelper().postOnLoadResource(request.url);
            }

            if (response != null && response.getData() == null) {
                // In this case the intercepted URLRequest job will simulate an empty response
                // which doesn't trigger the onReceivedError callback. For WebViewClassic
                // compatibility we synthesize that callback.  http://crbug.com/180950
                mAwContentsClient
                        .getCallbackHelper()
                        .postOnReceivedError(
                                request,
                                /* error description filled in by the glue layer */
                                new AwContentsClient.AwWebResourceError());
            }
        }

        if (!AwContentsIoThreadClientJni.get()
                .finishShouldInterceptRequest(
                        mRequestId,
                        new AwWebResourceInterceptResponse(
                                response, /* raisedException= */ false))) {
            throw new IllegalStateException("Request has already been responded to.");
        }
    }

    public void setAwContentsClient(AwContentsClient client) {
        mAwContentsClient = client;
    }

    public void clientRaisedException() {
        AwContentsIoThreadClientJni.get()
                .finishShouldInterceptRequest(
                        mRequestId,
                        new AwWebResourceInterceptResponse(null, /* raisedException= */ true));
    }
}
