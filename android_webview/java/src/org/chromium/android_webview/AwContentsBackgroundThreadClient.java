// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.android_webview.AwContentsClient.AwWebResourceRequest;
import org.chromium.android_webview.common.Lifetime;
import org.chromium.base.Log;

/**
 * Delegate for handling callbacks. All methods are called on the background thread.
 * "Background" means something that isn't UI or IO.
 */
@Lifetime.WebView
@JNINamespace("android_webview")
public abstract class AwContentsBackgroundThreadClient {
    private static final String TAG = "AwBgThreadClient";

    public abstract void shouldInterceptRequest(
            AwWebResourceRequest request, WebResponseCallback callback);

    // Protected methods ---------------------------------------------------------------------------

    @CalledByNative
    private void shouldInterceptRequestFromNative(
            String url,
            boolean isMainFrame,
            boolean hasUserGesture,
            String method,
            String[] requestHeaderNames,
            String[] requestHeaderValues,
            int requestId) {
        AwWebResourceRequest request =
                new AwWebResourceRequest(
                        url,
                        isMainFrame,
                        hasUserGesture,
                        method,
                        requestHeaderNames,
                        requestHeaderValues);
        WebResponseCallback callback = new WebResponseCallback(requestId, request);
        try {
            shouldInterceptRequest(request, callback);
        } catch (Throwable e) {
            Log.e(
                    TAG,
                    "Client raised exception in shouldInterceptRequest. Re-throwing on UI thread.");

            AwThreadUtils.postToUiThreadLooper(
                    () -> {
                        Log.e(TAG, "The following exception was raised by shouldInterceptRequest:");
                        throw e;
                    });

            callback.clientRaisedException();
        }
    }
}
