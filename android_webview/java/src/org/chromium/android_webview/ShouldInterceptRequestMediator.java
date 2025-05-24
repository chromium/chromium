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
import org.jni_zero.JniType;

import org.chromium.android_webview.common.AwFeatures;
import org.chromium.android_webview.common.Lifetime;
import org.chromium.base.JniOnceCallback;
import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.lang.reflect.Method;

/**
 * This class manages the shouldInterceptRequest callback, keeping track of when it's set and taking
 * care of thread safety.
 */
@Lifetime.WebView
@JNINamespace("android_webview")
@NullMarked
public abstract class ShouldInterceptRequestMediator {
    private static final String TAG = "shouldIntReqMed";

    // Used to record Android.WebView.ShouldInterceptRequest.DidOverride.
    @IntDef({
        ShouldInterceptRequestOverridden.AW_CONTENTS_NOT_OVERRIDDEN,
        ShouldInterceptRequestOverridden.AW_CONTENTS_OVERRIDDEN,
        ShouldInterceptRequestOverridden.AW_CONTENTS_ERROR,
        ShouldInterceptRequestOverridden.SERVICE_WORKER_NULL,
        ShouldInterceptRequestOverridden.SERVICE_WORKER_NON_NULL,
        ShouldInterceptRequestOverridden.COUNT
    })
    private @interface ShouldInterceptRequestOverridden {
        int AW_CONTENTS_NOT_OVERRIDDEN = 0;
        int AW_CONTENTS_OVERRIDDEN = 1;
        int AW_CONTENTS_ERROR = 2;
        int SERVICE_WORKER_NULL = 3;
        int SERVICE_WORKER_NON_NULL = 4;
        int COUNT = 5;
    }

    // If the embedder hasn't overridden WebViewClient#shouldInterceptRequest (or
    // ServiceWorkerClient#shouldInterceptRequest), we don't need to call it (and pay for the thread
    // hops).
    private volatile boolean mCanSkipSyncShouldInterceptRequest = true;

    // The default video poster functionality is implemented on top of shouldInterceptRequest. Even
    // if the developer hasn't overridden shouldInterceptRequest, we shouldn't skip it for the
    // default video poster URL.
    @Nullable private volatile String mNoSkipUrl;

    @AnyThread
    public boolean canSkipShouldInterceptRequest(String url) {
        // A user is only put into an experiment group when the feature is checked. By having the
        // feature check be the last clause in the conditional our experiment will only involve
        // users for whom we actually skip shouldInterceptRequest, and so we can see the benefits
        // of this optimization without it being diluted by all the users for whom
        // shouldInterceptRequest will need to be called anyway.
        return mCanSkipSyncShouldInterceptRequest
                && !url.equals(mNoSkipUrl)
                && AwFeatureMap.isEnabled(
                        AwFeatures.WEBVIEW_SHORT_CIRCUIT_SHOULD_INTERCEPT_REQUEST);
    }

    @AnyThread
    public void onWebViewClientUpdated(@Nullable WebViewClient client) {
        try {
            boolean overrides = overridesShouldInterceptRequest(client);
            recordOverridden(
                    overrides
                            ? ShouldInterceptRequestOverridden.AW_CONTENTS_OVERRIDDEN
                            : ShouldInterceptRequestOverridden.AW_CONTENTS_NOT_OVERRIDDEN);
            mCanSkipSyncShouldInterceptRequest = !overrides;
        } catch (NoSuchMethodException e) {
            // If something goes wrong, default to `false` because the consequences of wrongly being
            // false is suboptimal performance, whereas the consequences of wrongly being true is
            // correctness issues (shouldInterceptRequest not being called when it should be).
            recordOverridden(ShouldInterceptRequestOverridden.AW_CONTENTS_ERROR);
            mCanSkipSyncShouldInterceptRequest = false;
        }
    }

    @AnyThread
    public void onServiceWorkerClientUpdated(@Nullable AwServiceWorkerClient client) {
        // Figuring out whether the developer has overridden
        // ServiceWorkerClient#shouldInterceptRequest is tricky because they may be using
        // ServiceWorkerClientCompat from the support library, which in turn has two issues:
        //
        // - On Android N+, it will be implemented as a FrameworkServiceWorkerClient, which
        //   overrides shouldInterceptRequest, but doesn't necessarily do anything.
        // - On Android pre-N, it will be based on Boundary Interfaces, which make checking if it
        //   has been overridden more involved.
        //
        // Seeing as the only method on ServiceWorkerClient is shouldInterceptRequest, we're going
        // to assume that anyone providing a ServiceWorkerClient has overridden it.

        mCanSkipSyncShouldInterceptRequest = (client == null);
        recordOverridden(
                mCanSkipSyncShouldInterceptRequest
                        ? ShouldInterceptRequestOverridden.SERVICE_WORKER_NULL
                        : ShouldInterceptRequestOverridden.SERVICE_WORKER_NON_NULL);
    }

    @AnyThread
    public void setNoSkipUrl(@Nullable String url) {
        mNoSkipUrl = url;
    }

    /**
     * Allows the implementation to deal with the request. This will happen on a background thread
     * (meaning it isn't the UI or IO thread).
     */
    @AnyThread // @AnyThread implies the method needs to be threadsafe, I'd use @BackgroundThread
    // if it existed.
    public abstract void shouldInterceptRequest(
            AwWebResourceRequest request, WebResponseCallback responseCallback);

    // Protected methods ---------------------------------------------------------------------------

    @AnyThread
    @CalledByNative
    private void shouldInterceptRequestFromNative(
            @JniType("android_webview::AwWebResourceRequest") AwWebResourceRequest request,
            JniOnceCallback<AwWebResourceInterceptResponse> responseCallback) {
        WebResponseCallback callback = new WebResponseCallback(request, responseCallback);
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

    private static void recordOverridden(@ShouldInterceptRequestOverridden int value) {
        RecordHistogram.recordEnumeratedHistogram(
                "Android.WebView.ShouldInterceptRequest.DidOverride",
                value,
                ShouldInterceptRequestOverridden.COUNT);
    }

    @VisibleForTesting
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
