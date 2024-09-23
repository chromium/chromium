// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import android.content.Context;
import android.os.Build;
import android.webkit.WebResourceResponse;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.webkit.WebViewDelegate;

import androidx.annotation.Nullable;

import org.chromium.android_webview.AwContentsClient;
import org.chromium.android_webview.AwHistogramRecorder;
import org.chromium.android_webview.AwRenderProcess;
import org.chromium.android_webview.SafeBrowsingAction;
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
    // The WebView instance that this adapter is serving.
    protected final WebView mWebView;
    // The WebView delegate object that provides access to required framework APIs.
    protected final WebViewDelegate mWebViewDelegate;
    // The Context to use. This is different from mWebView.getContext(), which should not be used.
    protected final Context mContext;
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
    SharedWebViewContentsClientAdapter(
            WebView webView, WebViewDelegate webViewDelegate, Context context) {
        if (webView == null) {
            throw new IllegalArgumentException("webView can't be null.");
        }
        if (webViewDelegate == null) {
            throw new IllegalArgumentException("delegate can't be null.");
        }
        if (context == null) {
            throw new IllegalArgumentException("context can't be null.");
        }

        mWebView = webView;
        mWebViewDelegate = webViewDelegate;
        mContext = context;
        mSupportLibClient = new SupportLibWebViewContentsClientAdapter();
    }

    void setWebViewClient(WebViewClient client) {
        mWebViewClient = client;
        mSupportLibClient.setWebViewClient(client);
    }

    /** @see AwContentsClient#hasWebViewClient. */
    @Override
    public final boolean hasWebViewClient() {
        return mWebViewClient != SharedWebViewChromium.sNullWebViewClient;
    }

    /** @see AwContentsClient#shouldOverrideUrlLoading(AwContentsClient.AwWebResourceRequest) */
    @Override
    public final boolean shouldOverrideUrlLoading(AwContentsClient.AwWebResourceRequest request) {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICallback.WebViewClient.shouldOverrideUrlLoading")) {
            if (TRACE) Log.i(TAG, "shouldOverrideUrlLoading=" + request.url);
            boolean result;
            if (mSupportLibClient.isFeatureAvailable(Features.SHOULD_OVERRIDE_WITH_REDIRECTS)) {
                result =
                        mSupportLibClient.shouldOverrideUrlLoading(
                                mWebView, new WebResourceRequestAdapter(request));
            } else {
                result =
                        mWebViewClient.shouldOverrideUrlLoading(
                                mWebView, new WebResourceRequestAdapter(request));
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
                mSupportLibClient.onPageCommitVisible(mWebView, url);
            } else {
                mWebViewClient.onPageCommitVisible(mWebView, url);
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
            if (error.description == null || error.description.isEmpty()) {
                // ErrorStrings is @hidden, so we can't do this in AwContents.  Normally the net/
                // layer will set a valid description, but for synthesized callbacks (like in the
                // case for intercepted requests) AwContents will pass in null.
                error.description = mWebViewDelegate.getErrorString(mContext, error.errorCode);
            }
            if (TRACE) Log.i(TAG, "onReceivedError=" + request.url);
            if (mSupportLibClient.isFeatureAvailable(Features.RECEIVE_WEB_RESOURCE_ERROR)) {
                mSupportLibClient.onReceivedError(
                        mWebView, new WebResourceRequestAdapter(request), error);
            } else {
                mWebViewClient.onReceivedError(
                        mWebView,
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
                        mWebView, new WebResourceRequestAdapter(request), threatType, callback);
            } else if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O_MR1) {
                GlueApiHelperForOMR1.onSafeBrowsingHit(
                        mWebViewClient, mWebView, request, threatType, callback);

            } else {
                callback.onResult(
                        new AwSafeBrowsingResponse(
                                SafeBrowsingAction.SHOW_INTERSTITIAL, /* reporting= */ true));
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
            if (TRACE) Log.i(TAG, "onReceivedHttpError=" + request.url);
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
                        mWebView,
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
                        mWebView,
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
            mWebViewRendererClientAdapter.onRendererUnresponsive(mWebView, renderProcess);
        }
    }

    @Override
    public void onRendererResponsive(final AwRenderProcess renderProcess) {
        if (mWebViewRendererClientAdapter != null) {
            mWebViewRendererClientAdapter.onRendererResponsive(mWebView, renderProcess);
        }
    }
}
