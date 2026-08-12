// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import android.content.Context;
import android.webkit.WebResourceResponse;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.webkit.WebViewDelegate;

import androidx.annotation.AnyThread;
import androidx.annotation.Nullable;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwContentsClient;
import org.chromium.android_webview.AwHistogramRecorder;
import org.chromium.android_webview.AwRenderProcess;
import org.chromium.android_webview.AwWebResourceError;
import org.chromium.android_webview.AwWebResourceRequest;
import org.chromium.android_webview.safe_browsing.AwSafeBrowsingResponse;
import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.TraceEvent;
import org.chromium.components.embedder_support.util.WebResourceResponseInfo;
import org.chromium.support_lib_boundary.util.Features;
import org.chromium.support_lib_callback_glue.SupportLibWebViewContentsClientAdapter;

/** Partial adapter for AwContentsClient methods that may be handled by either glue layer. */
abstract class SharedWebViewContentsClientAdapter extends AwContentsClient {
    // TAG is chosen for consistency with classic webview tracing.
    protected static final String TAG = "WebViewCallback";
    // Enables API callback tracing
    protected static final boolean TRACE = false;
    // The WebView instance that this adapter is serving (dynamically resolved).
    protected final AwContents mAwContents;
    // The WebView delegate object that provides access to required framework APIs.
    protected final WebViewDelegate mWebViewDelegate;
    // A reference to the current WebViewClient associated with this WebView.
    protected WebViewClient mWebViewClient = SharedWebViewChromium.sNullWebViewClient;
    // Some callbacks will be forwarded to this client for apps using the support library.
    private final SupportLibWebViewContentsClientAdapter mSupportLibClient;

    private @Nullable SharedWebViewRendererClientAdapter mWebViewRendererClientAdapter;

    /**
     * Adapter constructor.
     *
     * @param webView the {@link WebView} instance that this adapter is serving.
     */
    SharedWebViewContentsClientAdapter(AwContents awContents, WebViewDelegate webViewDelegate) {
        if (awContents == null) {
            throw new IllegalArgumentException("awContents can't be null.");
        }
        if (webViewDelegate == null) {
            throw new IllegalArgumentException("delegate can't be null.");
        }

        mAwContents = awContents;
        mWebViewDelegate = webViewDelegate;
        mSupportLibClient = new SupportLibWebViewContentsClientAdapter();
    }

    @AnyThread
    public WebView getWebView() {
        return (WebView) mAwContents.getPrimaryContainerView();
    }

    public Context getContext() {
        return mAwContents.getProvidedContext();
    }

    void setWebViewClient(WebViewClient client) {
        mWebViewClient = client;
        mSupportLibClient.setWebViewClient(client);
    }

    WebViewClient getWebViewClient() {
        return mWebViewClient;
    }

    /** @see AwContentsClient#hasWebViewClient. */
    @Override
    public final boolean hasWebViewClient() {
        return mWebViewClient != SharedWebViewChromium.sNullWebViewClient;
    }

    /**
     * @see AwContentsClient#shouldOverrideUrlLoading(AwWebResourceRequest)
     */
    @Override
    public final boolean shouldOverrideUrlLoading(AwWebResourceRequest request) {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICallback.WebViewClient.shouldOverrideUrlLoading")) {
            if (TRACE) Log.i(TAG, "shouldOverrideUrlLoading=" + request.getUrl());
            boolean result;
            if (mSupportLibClient.isFeatureAvailable(Features.SHOULD_OVERRIDE_WITH_REDIRECTS)) {
                result =
                        mSupportLibClient.shouldOverrideUrlLoading(
                                getWebView(), new WebResourceRequestAdapter(request));
            } else {
                result =
                        mWebViewClient.shouldOverrideUrlLoading(
                                getWebView(), new WebResourceRequestAdapter(request));
            }
            if (TRACE) Log.i(TAG, "shouldOverrideUrlLoading result=" + result);

            // Record UMA for shouldOverrideUrlLoading.
            AwHistogramRecorder.recordCallbackInvocation(
                    AwHistogramRecorder.WebViewCallbackType.SHOULD_OVERRIDE_URL_LOADING);

            return result;
        }
    }

    /** @see ContentViewClient#onPageCommitVisible(String) */
    @Override
    public final void onPageCommitVisible(String url) {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICallback.WebViewClient.onPageCommitVisible")) {
            if (TRACE) Log.i(TAG, "onPageCommitVisible=" + url);
            if (mSupportLibClient.isFeatureAvailable(Features.VISUAL_STATE_CALLBACK)) {
                mSupportLibClient.onPageCommitVisible(getWebView(), url);
            } else {
                mWebViewClient.onPageCommitVisible(getWebView(), url);
            }

            // Record UMA for onPageCommitVisible.
            AwHistogramRecorder.recordCallbackInvocation(
                    AwHistogramRecorder.WebViewCallbackType.ON_PAGE_COMMIT_VISIBLE);

            // Otherwise, the API does not exist, so do nothing.
        }
    }

    /** @see ContentViewClient#onReceivedError(AwWebResourceRequest,AwWebResourceError) */
    @Override
    public void onReceivedError(AwWebResourceRequest request, AwWebResourceError error) {
        try (TraceEvent event = TraceEvent.scoped("WebViewContentsClientAdapter.onReceivedError")) {
            AwHistogramRecorder.recordCallbackInvocation(
                    AwHistogramRecorder.WebViewCallbackType.ON_RECEIVED_ERROR);
            if (error.getDescription() == null || error.getDescription().isEmpty()) {
                // ErrorStrings is @hidden, so we can't do this in AwContents.  Normally the net/
                // layer will set a valid description, but for synthesized callbacks (like in the
                // case for intercepted requests) AwContents will pass in null.
                error.setDescription(
                        mWebViewDelegate.getErrorString(getContext(), error.getWebviewError()));
            }
            if (TRACE) Log.i(TAG, "onReceivedError=" + request.getUrl());
            if (mSupportLibClient.isFeatureAvailable(Features.RECEIVE_WEB_RESOURCE_ERROR)) {
                mSupportLibClient.onReceivedError(
                        getWebView(), new WebResourceRequestAdapter(request), error);
            } else {
                mWebViewClient.onReceivedError(
                        getWebView(),
                        new WebResourceRequestAdapter(request),
                        new WebResourceErrorAdapter(error));
            }
        }
    }

    @Override
    public void onSafeBrowsingHit(
            AwWebResourceRequest request,
            int threatType,
            final Callback<AwSafeBrowsingResponse> callback) {
        try (TraceEvent event =
                TraceEvent.scoped("WebViewContentsClientAdapter.onSafeBrowsingHit")) {
            AwHistogramRecorder.recordCallbackInvocation(
                    AwHistogramRecorder.WebViewCallbackType.ON_SAFE_BROWSING_HIT);
            if (mSupportLibClient.isFeatureAvailable(Features.SAFE_BROWSING_HIT)) {
                mSupportLibClient.onSafeBrowsingHit(
                        getWebView(), new WebResourceRequestAdapter(request), threatType, callback);
            } else {
                mWebViewClient.onSafeBrowsingHit(
                        getWebView(),
                        new WebResourceRequestAdapter(request),
                        threatType,
                        new SafeBrowsingResponseAdapter(callback));
            }
        }
    }

    @Override
    public void onReceivedHttpError(
            AwWebResourceRequest request, WebResourceResponseInfo response) {
        try (TraceEvent event =
                TraceEvent.scoped("WebViewContentsClientAdapter.onReceivedHttpError")) {
            AwHistogramRecorder.recordCallbackInvocation(
                    AwHistogramRecorder.WebViewCallbackType.ON_RECEIVED_HTTP_ERROR);
            if (TRACE) Log.i(TAG, "onReceivedHttpError=" + request.getUrl());
            if (mSupportLibClient.isFeatureAvailable(Features.RECEIVE_HTTP_ERROR)) {
                // Note: we use the @SystemApi constructor here because it relaxes several
                // requirements:
                // * response.getReasonPhrase() may legitimately be empty because HTTP/2 removed
                //   Reason-Phrase from the spec (https://crbug.com/925887).
                // * response.getStatusCode() may be out of the valid range if the web server is not
                //   obeying the HTTP spec (ex. http://b/235960500).
                //
                // Immutability is not strictly necessary, but apps should not not need to modify
                // the WebResourceResponse received in this callback (they can always construct
                // their own instance).
                mSupportLibClient.onReceivedHttpError(
                        getWebView(),
                        new WebResourceRequestAdapter(request),
                        new WebResourceResponse(
                                /* immutable= */ true,
                                response.getMimeType(),
                                response.getCharset(),
                                response.getStatusCode(),
                                response.getReasonPhrase(),
                                response.getResponseHeaders(),
                                response.getData()));
            } else {
                mWebViewClient.onReceivedHttpError(
                        getWebView(),
                        new WebResourceRequestAdapter(request),
                        new WebResourceResponse(
                                /* immutable= */ true,
                                response.getMimeType(),
                                response.getCharset(),
                                response.getStatusCode(),
                                response.getReasonPhrase(),
                                response.getResponseHeaders(),
                                response.getData()));
            }
            // Otherwise, the API does not exist, so do nothing.
        }
    }

    void setWebViewRendererClientAdapter(
            SharedWebViewRendererClientAdapter webViewRendererClientAdapter) {
        mWebViewRendererClientAdapter = webViewRendererClientAdapter;
    }

    SharedWebViewRendererClientAdapter getWebViewRendererClientAdapter() {
        return mWebViewRendererClientAdapter;
    }

    @Override
    public void onRendererUnresponsive(final AwRenderProcess renderProcess) {
        if (mWebViewRendererClientAdapter != null) {
            mWebViewRendererClientAdapter.onRendererUnresponsive(getWebView(), renderProcess);
        }
    }

    @Override
    public void onRendererResponsive(final AwRenderProcess renderProcess) {
        if (mWebViewRendererClientAdapter != null) {
            mWebViewRendererClientAdapter.onRendererResponsive(getWebView(), renderProcess);
        }
    }
}
