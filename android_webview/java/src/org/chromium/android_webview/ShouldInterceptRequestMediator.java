// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import androidx.annotation.AnyThread;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.android_webview.AwContentsClient.AwWebResourceRequest;
import org.chromium.android_webview.common.Lifetime;
import org.chromium.base.Log;
import org.chromium.build.annotations.Nullable;

import java.util.concurrent.atomic.AtomicReference;

/**
 * This class manages the shouldInterceptRequest callback, keeping track of when it's set and taking
 * care of thread safety.
 */
@Lifetime.WebView
@JNINamespace("android_webview")
public abstract class ShouldInterceptRequestMediator {
    private static final String TAG = "shouldIntReqMed";

    private final AtomicReference<AsyncShouldInterceptRequestCallback> mAsyncCallback =
            new AtomicReference<>();

    @AnyThread
    public void setAsyncCallback(@Nullable AsyncShouldInterceptRequestCallback callback) {
        mAsyncCallback.set(callback);
    }

    /**
     * Allows the implementation to deal with the request. This will happen on a background thread
     * (meaning it isn't the UI or IO thread).
     */
    @AnyThread // @AnyThread implies the method needs to be threadsafe, I'd use @BackgroundThread
    // if it existed.
    public abstract void shouldInterceptRequest(
            AwWebResourceRequest request,
            WebResponseCallback responseCallback,
            AsyncShouldInterceptRequestCallback asyncShouldInterceptRequestCallback);

    // Protected methods ---------------------------------------------------------------------------

    @AnyThread
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
            shouldInterceptRequest(request, callback, mAsyncCallback.get());
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
