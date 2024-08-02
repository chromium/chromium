// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_boundary;

import android.webkit.WebResourceRequest;
import android.webkit.WebResourceResponse;
import android.webkit.WebView;

import java.lang.reflect.InvocationHandler;

/** Boundary interface for WebViewClientCompat. */
public interface WebViewClientBoundaryInterface extends FeatureFlagHolderBoundaryInterface {
    void onPageCommitVisible(WebView view, String url);

    void onReceivedError(
            WebView view,
            WebResourceRequest request,
            /* WebResourceError */ InvocationHandler error);

    void onReceivedHttpError(
            WebView view, WebResourceRequest request, WebResourceResponse errorResponse);

    void onSafeBrowsingHit(
            WebView view,
            WebResourceRequest request,
            int threatType,
            /* SafeBrowsingResponse */ InvocationHandler callback);

    boolean shouldOverrideUrlLoading(WebView view, WebResourceRequest request);
}
