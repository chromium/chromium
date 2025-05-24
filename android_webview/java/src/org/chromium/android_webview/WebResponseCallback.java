// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import org.jni_zero.JNINamespace;

import org.chromium.base.JniOnceCallback;
import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.embedder_support.util.WebResourceResponseInfo;

import java.util.concurrent.atomic.AtomicBoolean;

/**
 * WebResponseCallback can be called from any thread, and can be called either during
 * shouldInterceptRequestAsync or later. Each request must be responded to exactly once; calling the
 * same callback object multiple times is not permitted and the implementation should always
 * eventually call the callback.
 */
@JNINamespace("android_webview")
@NullMarked
public final class WebResponseCallback {
    private static final String TAG = "WebRspnsCllbck";
    private @Nullable AwContentsClient mAwContentsClient;
    private final AwWebResourceRequest mRequest;

    private final JniOnceCallback<AwWebResourceInterceptResponse> mResponseCallback;

    /** Callback from native code should only be called once. */
    private final AtomicBoolean mInterceptCalled = new AtomicBoolean(false);

    public WebResponseCallback(
            AwWebResourceRequest request,
            final JniOnceCallback<AwWebResourceInterceptResponse> responseCallback) {
        mRequest = request;
        mResponseCallback = responseCallback;
    }

    public void intercept(@Nullable WebResourceResponseInfo response) {
        if (mInterceptCalled.getAndSet(true)) {
            throw new IllegalStateException("Request has already been responded to.");
        }
        if (mAwContentsClient != null) {
            notifyContentsClientCallbackHelper(mAwContentsClient.getCallbackHelper(), response);
        }
        mResponseCallback.onResult(new AwWebResourceInterceptResponse(response, false));
    }

    private void notifyContentsClientCallbackHelper(
            AwContentsClientCallbackHelper callbackHelper,
            @Nullable WebResourceResponseInfo response) {
        if (response == null) {
            callbackHelper.postOnLoadResource(mRequest.getUrl());
        } else if (response.getData() == null) {
            // In this case the intercepted URLRequest job will simulate an empty response
            // which doesn't trigger the onReceivedError callback. For WebViewClassic
            // compatibility we synthesize that callback.  http://crbug.com/180950
            callbackHelper.postOnReceivedError(
                    mRequest,
                    /* error description filled in by the glue layer */
                    new AwContentsClient.AwWebResourceError());
        }
    }

    public void setAwContentsClient(AwContentsClient client) {
        mAwContentsClient = client;
    }

    public void clientRaisedException() {
        if (mInterceptCalled.getAndSet(true)) {
            // Prevent the underlying callback from being invoked twice, which is an
            // error.
            return;
        }
        mResponseCallback.onResult(
                new AwWebResourceInterceptResponse(null, /* raisedException= */ true));
    }

    // Performs cleanup in the possible event where the callback object goes unused.
    @SuppressWarnings("Finalize")
    @Override
    protected void finalize() throws Throwable {
        try {
            boolean intercepted = mInterceptCalled.get();
            RecordHistogram.recordBooleanHistogram(
                    "Android.WebView.ShouldInterceptRequest.Async.CallbackLeakedWithoutResponse",
                    !intercepted);
            if (!intercepted) {
                Log.e(
                        TAG,
                        "Client's shouldInterceptRequestAsync implementation did not respond for "
                                + mRequest.getUrl());
                clientRaisedException();
            }
        } finally {
            super.finalize(); // Call superclass finalize() to ensure proper cleanup.
        }
    }
}
