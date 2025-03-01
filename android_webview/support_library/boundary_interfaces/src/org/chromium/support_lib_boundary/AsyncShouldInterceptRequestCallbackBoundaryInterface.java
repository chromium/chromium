// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_boundary;

import android.webkit.WebResourceRequest;
import android.webkit.WebResourceResponse;
import android.webkit.WebView;

import org.jspecify.annotations.NullMarked;

import java.lang.reflect.InvocationHandler;

/** Boundary interface for org.chromium.android_webview.AsyncShouldInterceptRequestCallback. */
@NullMarked
public interface AsyncShouldInterceptRequestCallbackBoundaryInterface {
    void shouldInterceptRequestAsync(
            WebView webview,
            WebResourceRequest request,
            /* WebResponseCallback */ InvocationHandler responseCallback);

    /** Boundary interface for org.chromium.android_webview.WebResponseCallback. */
    @NullMarked
    public interface WebResponseCallbackBoundaryInterface {
        void intercept(WebResourceResponse response);

        void doNotIntercept();
    }
}
