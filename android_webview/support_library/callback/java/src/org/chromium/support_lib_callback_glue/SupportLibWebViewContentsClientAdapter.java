// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_callback_glue;

import android.webkit.WebResourceRequest;
import android.webkit.WebResourceResponse;
import android.webkit.WebView;
import android.webkit.WebViewClient;

import androidx.annotation.Nullable;

import org.chromium.android_webview.AwContentsClient.AwWebResourceError;
import org.chromium.android_webview.safe_browsing.AwSafeBrowsingResponse;
import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.ScopedSysTraceEvent;
import org.chromium.support_lib_boundary.SafeBrowsingResponseBoundaryInterface;
import org.chromium.support_lib_boundary.WebResourceErrorBoundaryInterface;
import org.chromium.support_lib_boundary.WebViewClientBoundaryInterface;
import org.chromium.support_lib_boundary.util.BoundaryInterfaceReflectionUtil;
import org.chromium.support_lib_boundary.util.Features;

import java.lang.reflect.InvocationHandler;

/**
 * Support library glue version of WebViewContentsClientAdapter. This should be 1:1 with
 * WebViewContentsClientAdapter.
 */
public class SupportLibWebViewContentsClientAdapter {
    private static final String WEBVIEW_CLIENT_COMPAT_NAME = "androidx.webkit.WebViewClientCompat";
    private static final String[] EMPTY_FEATURE_LIST = new String[0];

    private static final String COMPAT_CLIENT_HISTOGRAM =
            "Android.WebView.SupportLibrary.ClientIsCompat";

    // If {@code null}, this indicates the WebViewClient is not a WebViewClientCompat. Otherwise,
    // this is a Proxy for the WebViewClientCompat.
    @Nullable
    private WebViewClientBoundaryInterface mWebViewClient;
    private String[] mWebViewClientSupportedFeatures;

    public SupportLibWebViewContentsClientAdapter() {
        mWebViewClientSupportedFeatures = EMPTY_FEATURE_LIST;
    }

    public void setWebViewClient(WebViewClient possiblyCompatClient) {
        try (ScopedSysTraceEvent event = ScopedSysTraceEvent.scoped(
                     "SupportLibWebViewContentsClientAdapter.setWebViewClient")) {
            mWebViewClient = convertCompatClient(possiblyCompatClient);
            mWebViewClientSupportedFeatures = mWebViewClient == null
                    ? EMPTY_FEATURE_LIST
                    : mWebViewClient.getSupportedFeatures();

            // We ignore the case where the client is set to null, since this is often done by
            // WebView's internal logic (such as during destroy()), and would otherwise skew data.
            if (possiblyCompatClient != null) {
                RecordHistogram.recordBooleanHistogram(
                        COMPAT_CLIENT_HISTOGRAM, mWebViewClient != null);
            }
        }
    }

    @Nullable
    private WebViewClientBoundaryInterface convertCompatClient(WebViewClient possiblyCompatClient) {
        if (!BoundaryInterfaceReflectionUtil.instanceOfInOwnClassLoader(
                    possiblyCompatClient, WEBVIEW_CLIENT_COMPAT_NAME)) {
            return null;
        }

        InvocationHandler handler =
                BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(possiblyCompatClient);

        return BoundaryInterfaceReflectionUtil.castToSuppLibClass(
                WebViewClientBoundaryInterface.class, handler);
    }

    /**
     * Indicates whether this client can handle the callback(s) assocated with {@param featureName}.
     * This should be called with the correct feature name before invoking the corresponding
     * callback, and the callback must not be called if this returns {@code false} for the feature.
     *
     * @param featureName the feature for the desired callback.
     * @return {@code true} if this client can handle the feature.
     */
    public boolean isFeatureAvailable(String featureName) {
        if (mWebViewClient == null) return false;
        return BoundaryInterfaceReflectionUtil.containsFeature(
                mWebViewClientSupportedFeatures, featureName);
    }

    public void onPageCommitVisible(WebView webView, String url) {
        assert isFeatureAvailable(Features.VISUAL_STATE_CALLBACK);
        mWebViewClient.onPageCommitVisible(webView, url);
    }

    public void onReceivedError(
            WebView webView, WebResourceRequest request, final AwWebResourceError error) {
        assert isFeatureAvailable(Features.RECEIVE_WEB_RESOURCE_ERROR);
        WebResourceErrorBoundaryInterface supportLibError = new SupportLibWebResourceError(error);
        InvocationHandler errorHandler =
                BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(supportLibError);
        mWebViewClient.onReceivedError(webView, request, errorHandler);
    }

    public void onReceivedHttpError(
            WebView webView, WebResourceRequest request, WebResourceResponse response) {
        assert isFeatureAvailable(Features.RECEIVE_HTTP_ERROR);
        mWebViewClient.onReceivedHttpError(webView, request, response);
    }

    public void onSafeBrowsingHit(WebView webView, WebResourceRequest request, int threatType,
            Callback<AwSafeBrowsingResponse> callback) {
        assert isFeatureAvailable(Features.SAFE_BROWSING_HIT);
        SafeBrowsingResponseBoundaryInterface supportLibResponse =
                new SupportLibSafeBrowsingResponse(callback);
        InvocationHandler responseHandler =
                BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(supportLibResponse);
        mWebViewClient.onSafeBrowsingHit(webView, request, threatType, responseHandler);
    }

    public boolean shouldOverrideUrlLoading(WebView webView, WebResourceRequest request) {
        assert isFeatureAvailable(Features.SHOULD_OVERRIDE_WITH_REDIRECTS);
        return mWebViewClient.shouldOverrideUrlLoading(webView, request);
    }
}
