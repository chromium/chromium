// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_glue;

import android.webkit.WebView;

import com.android.webview.chromium.WebResourceRequestAdapter;

import org.chromium.android_webview.AsyncShouldInterceptRequestCallback;
import org.chromium.android_webview.AwContentsClient.AwWebResourceRequest;
import org.chromium.android_webview.WebResponseCallback;
import org.chromium.android_webview.common.Lifetime;
import org.chromium.support_lib_boundary.AsyncShouldInterceptRequestCallbackBoundaryInterface;
import org.chromium.support_lib_boundary.util.BoundaryInterfaceReflectionUtil;

import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Proxy;

/**
 * Support library glue AsyncShouldInterceptRequestCallback.
 *
 * <p>A new instance of this class is created transiently for every shared library WebViewCompat
 * call. Do not store state here.
 */
@Lifetime.Temporary
class AsyncShouldInterceptRequestCallbackAdapter implements AsyncShouldInterceptRequestCallback {
    private static final String TAG = "AShldIntCllbckAdptr";
    private final AsyncShouldInterceptRequestCallbackBoundaryInterface mImpl;
    private final WebView mWebView;

    public AsyncShouldInterceptRequestCallbackAdapter(
            WebView view, /* AsyncShouldInterceptRequestCallback */ InvocationHandler handler) {
        mWebView = view;
        mImpl =
                BoundaryInterfaceReflectionUtil.castToSuppLibClass(
                        AsyncShouldInterceptRequestCallbackBoundaryInterface.class, handler);
    }

    public /* AsyncShouldInterceptRequestCallback */ InvocationHandler
            getSupportLibInvocationHandler() {
        return Proxy.getInvocationHandler(mImpl);
    }

    @Override
    public void shouldInterceptRequestAsync(
            AwWebResourceRequest request, WebResponseCallback callback) {
        mImpl.shouldInterceptRequestAsync(
                mWebView,
                new WebResourceRequestAdapter(request),
                BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                        new WebResponseCallbackAdapter(callback)));
    }
}
