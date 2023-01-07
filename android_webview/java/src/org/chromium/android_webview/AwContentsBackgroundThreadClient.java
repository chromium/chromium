// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import androidx.annotation.NonNull;

import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.components.embedder_support.util.WebResourceResponseInfo;

/**
 * Delegate for handling callbacks. All methods are called on the background thread.
 * "Background" means something that isn't UI or IO.
 */
@JNINamespace("android_webview")
public abstract class AwContentsBackgroundThreadClient {
    private static final String TAG = "AwBgThreadClient";

    public abstract WebResourceResponseInfo shouldInterceptRequest(
            AwContentsClient.AwWebResourceRequest request);

    // Protected methods ---------------------------------------------------------------------------

    @NonNull
    @CalledByNative
    private AwWebResourceInterceptResponse shouldInterceptRequestFromNative(String url,
            boolean isMainFrame, boolean hasUserGesture, String method, String[] requestHeaderNames,
            String[] requestHeaderValues) {
        try {
            return new AwWebResourceInterceptResponse(
                    shouldInterceptRequest(new AwContentsClient.AwWebResourceRequest(url,
                            isMainFrame, hasUserGesture, method, requestHeaderNames,
                            requestHeaderValues)),
                    /*raisedException=*/false);
        } catch (Throwable e) {
            Log.e(TAG,
                    "Client raised exception in shouldInterceptRequest. Re-throwing on UI thread.");

            AwThreadUtils.postToUiThreadLooper(() -> {
                Log.e(TAG, "The following exception was raised by shouldInterceptRequest:");
                throw e;
            });

            return new AwWebResourceInterceptResponse(null, /*raisedException=*/true);
        }
    }
}
