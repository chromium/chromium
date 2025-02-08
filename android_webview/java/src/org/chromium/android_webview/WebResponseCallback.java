// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import org.jni_zero.JNINamespace;

import org.chromium.android_webview.AwContentsClient.AwWebResourceRequest;
import org.chromium.components.embedder_support.util.WebResourceResponseInfo;

/**
 * WebResponseCallback can be called from any thread, and can be called either during
 * shouldInterceptRequestAsync or later. Each request must be responded to exactly once; calling the
 * same callback object multiple times is not permitted and the implementation should always
 * eventually call the callback.
 */
// TODO(crbug.com/395117566): Add a way to detect that the callback object has been "abandoned" (the
// app didn't call intercept and didn't keep a reference to it either) and then do something to
// clean it up; The handling should probably throw an exception when this happens.
@JNINamespace("android_webview")
public final class WebResponseCallback {
    private static final String TAG = "WebRspnsCllbck";
    private final int mRequestId;
    private AwContentsClient mAwContentsClient;
    private final AwWebResourceRequest mRequest;

    public WebResponseCallback(int requestId, AwWebResourceRequest request) {
        mRequestId = requestId;
        mRequest = request;
    }

    public void intercept(WebResourceResponseInfo response) {
        if (mAwContentsClient != null) {
            if (response == null) {
                mAwContentsClient.getCallbackHelper().postOnLoadResource(mRequest.url);
            }
            if (response != null && response.getData() == null) {
                // In this case the intercepted URLRequest job will simulate an empty response
                // which doesn't trigger the onReceivedError callback. For WebViewClassic
                // compatibility we synthesize that callback.  http://crbug.com/180950
                mAwContentsClient
                        .getCallbackHelper()
                        .postOnReceivedError(
                                mRequest,
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
