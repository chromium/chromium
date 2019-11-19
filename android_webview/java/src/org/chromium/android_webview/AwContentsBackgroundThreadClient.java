// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

/**
 * Delegate for handling callbacks. All methods are called on the background thread.
 * "Background" means something that isn't UI or IO.
 */
@JNINamespace("android_webview")
public abstract class AwContentsBackgroundThreadClient {
    private static final String TAG = "AwBgThreadClient";

    public abstract AwWebResourceResponse shouldInterceptRequest(
            AwContentsClient.AwWebResourceRequest request);

    // Protected methods ---------------------------------------------------------------------------

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
        } catch (Exception e) {
            Log.e(TAG,
                    "Client raised exception in shouldInterceptRequest. Re-throwing on UI thread.");

            ThreadUtils.getUiThreadHandler().post(new Runnable() {
                @Override
                public void run() {
                    Log.e(TAG, "The following exception was raised by shouldInterceptRequest:");
                    throw e;
                }
            });

            return new AwWebResourceInterceptResponse(null, /*raisedException=*/true);
        }
    }
}
