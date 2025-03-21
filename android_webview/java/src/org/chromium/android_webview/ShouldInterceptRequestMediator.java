// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.webkit.WebResourceRequest;
import android.webkit.WebView;
import android.webkit.WebViewClient;

import androidx.annotation.AnyThread;
import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.android_webview.common.AwFeatures;
import org.chromium.android_webview.common.Lifetime;
import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.Nullable;

import java.lang.reflect.Method;

/**
 * This class manages the shouldInterceptRequest callback, keeping track of when it's set and taking
 * care of thread safety.
 */
@Lifetime.WebView
@JNINamespace("android_webview")
public abstract class ShouldInterceptRequestMediator {
    private static final String TAG = "shouldIntReqMed";

    // Used to record Android.WebView.ShouldInterceptRequest.DidOverride.
    @IntDef({
        ShouldInterceptRequestOverridden.NOT_OVERRIDDEN,
        ShouldInterceptRequestOverridden.OVERRIDDEN,
        ShouldInterceptRequestOverridden.ERROR,
        ShouldInterceptRequestOverridden.COUNT
    })
    private @interface ShouldInterceptRequestOverridden {
        int NOT_OVERRIDDEN = 0;
        int OVERRIDDEN = 1;
        int ERROR = 2;
        int COUNT = 3;
    }

    @Nullable private volatile AsyncShouldInterceptRequestCallback mAsyncCallback;

    // If the embedder hasn't overridden WebViewClient#shouldInterceptRequest (or
    // ServiceWorkerClient#shouldInterceptRequest), we don't need to call it (and pay for the thread
    // hops).
    private volatile boolean mCanSkipSyncShouldInterceptRequest = true;

    // The default video poster functionality is implemented on top of shouldInterceptRequest. Even
    // if the developer hasn't overridden shouldInterceptRequest, we shouldn't skip it for the
    // default video poster URL.
    @Nullable private volatile String mNoSkipUrl;

    @AnyThread
    public void setAsyncCallback(@Nullable AsyncShouldInterceptRequestCallback callback) {
        mAsyncCallback = callback;
    }

    @AnyThread
    public boolean canSkipShouldInterceptRequest(String url) {
        if (!AwFeatureMap.isEnabled(AwFeatures.WEBVIEW_SHORT_CIRCUIT_SHOULD_INTERCEPT_REQUEST)) {
            return false;
        }
        return mCanSkipSyncShouldInterceptRequest
                && mAsyncCallback == null
                && !url.equals(mNoSkipUrl);
    }

    @AnyThread
    public void onWebViewClientUpdated(@Nullable WebViewClient client) {
        try {
            boolean overrides = overridesShouldInterceptRequest(client);
            recordOverridden(
                    overrides
                            ? ShouldInterceptRequestOverridden.OVERRIDDEN
                            : ShouldInterceptRequestOverridden.NOT_OVERRIDDEN);
            mCanSkipSyncShouldInterceptRequest = !overrides;
        } catch (NoSuchMethodException e) {
            // If something goes wrong, default to `false` because the consequences of wrongly being
            // false is suboptimal performance, whereas the consequences of wrongly being true is
            // correctness issues (shouldInterceptRequest not being called when it should be).
            recordOverridden(ShouldInterceptRequestOverridden.ERROR);
            mCanSkipSyncShouldInterceptRequest = false;
        }
    }

    @AnyThread
    public void setNoSkipUrl(String url) {
        mNoSkipUrl = url;
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
            shouldInterceptRequest(request, callback, mAsyncCallback);
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

    private static void recordOverridden(@ShouldInterceptRequestOverridden int value) {
        RecordHistogram.recordEnumeratedHistogram(
                "Android.WebView.ShouldInterceptRequest.DidOverride",
                value,
                ShouldInterceptRequestOverridden.COUNT);
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public static boolean overridesShouldInterceptRequest(@Nullable WebViewClient client)
            throws NoSuchMethodException {
        if (client == null) return false;

        Class<?> clientClass = client.getClass();

        // There are two overloads of WebViewClient#shouldInterceptRequest, one that takes a String
        // and one that takes a WebResourceRequest.
        Method shouldInterceptRequest1 =
                clientClass.getMethod("shouldInterceptRequest", WebView.class, String.class);
        Method shouldInterceptRequest2 =
                clientClass.getMethod(
                        "shouldInterceptRequest", WebView.class, WebResourceRequest.class);

        Class<?> baseClass = WebViewClient.class;

        return !shouldInterceptRequest1.getDeclaringClass().equals(baseClass)
                || !shouldInterceptRequest2.getDeclaringClass().equals(baseClass);
    }
}
