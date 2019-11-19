// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import android.content.Context;
import android.os.Build;
import android.webkit.WebResourceResponse;
import android.webkit.WebView;
import android.webkit.WebViewClient;

import androidx.annotation.Nullable;

import com.android.webview.chromium.WebViewDelegateFactory.WebViewDelegate;

import org.chromium.android_webview.AwContentsClient;
import org.chromium.android_webview.AwHistogramRecorder;
import org.chromium.android_webview.AwRenderProcess;
import org.chromium.android_webview.AwWebResourceResponse;
import org.chromium.android_webview.SafeBrowsingAction;
import org.chromium.android_webview.safe_browsing.AwSafeBrowsingResponse;
import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.TraceEvent;
import org.chromium.base.compat.ApiHelperForM;
import org.chromium.base.compat.ApiHelperForN;
import org.chromium.support_lib_boundary.util.Features;
import org.chromium.support_lib_callback_glue.SupportLibWebViewContentsClientAdapter;

/**
 * Partial adapter for AwContentsClient methods that may be handled by either glue layer.
 */
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

    /**
     * @see AwContentsClient#hasWebViewClient.
     */
    @Override
    public final boolean hasWebViewClient() {
        return mWebViewClient != SharedWebViewChromium.sNullWebViewClient;
    }

    /**
     * @see AwContentsClient#shouldOverrideUrlLoading(AwContentsClient.AwWebResourceRequest)
     */
    @Override
    public final boolean shouldOverrideUrlLoading(AwContentsClient.AwWebResourceRequest request) {
        try {
            TraceEvent.begin("WebViewContentsClientAdapter.shouldOverrideUrlLoading");
            if (TRACE) Log.i(TAG, "shouldOverrideUrlLoading=" + request.url);
            boolean result;
            if (mSupportLibClient.isFeatureAvailable(Features.SHOULD_OVERRIDE_WITH_REDIRECTS)) {
                result = mSupportLibClient.shouldOverrideUrlLoading(
                        mWebView, new WebResourceRequestAdapter(request));
            } else if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
                result = ApiHelperForN.shouldOverrideUrlLoading(
                        mWebViewClient, mWebView, new WebResourceRequestAdapter(request));
            } else {
                result = mWebViewClient.shouldOverrideUrlLoading(mWebView, request.url);
            }
            if (TRACE) Log.i(TAG, "shouldOverrideUrlLoading result=" + result);

            // Record UMA for shouldOverrideUrlLoading.
            AwHistogramRecorder.recordCallbackInvocation(
                    AwHistogramRecorder.WebViewCallbackType.SHOULD_OVERRIDE_URL_LOADING);

            return result;
        } finally {
            TraceEvent.end("WebViewContentsClientAdapter.shouldOverrideUrlLoading");
        }
    }

    /**
     * @see ContentViewClient#onPageCommitVisible(String)
     */
    @Override
    public final void onPageCommitVisible(String url) {
        try {
            TraceEvent.begin("WebViewContentsClientAdapter.onPageCommitVisible");
            if (TRACE) Log.i(TAG, "onPageCommitVisible=" + url);
            if (mSupportLibClient.isFeatureAvailable(Features.VISUAL_STATE_CALLBACK)) {
                mSupportLibClient.onPageCommitVisible(mWebView, url);
            } else if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
                ApiHelperForM.onPageCommitVisible(mWebViewClient, mWebView, url);
            }

            // Record UMA for onPageCommitVisible.
            AwHistogramRecorder.recordCallbackInvocation(
                    AwHistogramRecorder.WebViewCallbackType.ON_PAGE_COMMIT_VISIBLE);

            // Otherwise, the API does not exist, so do nothing.
        } finally {
            TraceEvent.end("WebViewContentsClientAdapter.onPageCommitVisible");
        }
    }

    /**
     * @see ContentViewClient#onReceivedError(int,String,String)
     */
    @Override
    public final void onReceivedError(int errorCode, String description, String failingUrl) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) return;

        // This event is handled by the support lib in {@link #onReceivedError2}.
        if (mSupportLibClient.isFeatureAvailable(Features.RECEIVE_WEB_RESOURCE_ERROR)) return;

        try {
            TraceEvent.begin("WebViewContentsClientAdapter.onReceivedError");
            if (description == null || description.isEmpty()) {
                // ErrorStrings is @hidden, so we can't do this in AwContents.  Normally the net/
                // layer will set a valid description, but for synthesized callbacks (like in the
                // case for intercepted requests) AwContents will pass in null.
                description = mWebViewDelegate.getErrorString(mContext, errorCode);
            }
            if (TRACE) Log.i(TAG, "onReceivedError=" + failingUrl);
            mWebViewClient.onReceivedError(mWebView, errorCode, description, failingUrl);
        } finally {
            TraceEvent.end("WebViewContentsClientAdapter.onReceivedError");
        }
    }

    /**
     * @see ContentViewClient#onReceivedError(AwWebResourceRequest,AwWebResourceError)
     */
    @Override
    public void onReceivedError2(AwWebResourceRequest request, AwWebResourceError error) {
        try {
            TraceEvent.begin("WebViewContentsClientAdapter.onReceivedError");
            if (error.description == null || error.description.isEmpty()) {
                // ErrorStrings is @hidden, so we can't do this in AwContents.  Normally the net/
                // layer will set a valid description, but for synthesized callbacks (like in the
                // case for intercepted requests) AwContents will pass in null.
                error.description = mWebViewDelegate.getErrorString(mContext, error.errorCode);
            }
            if (TRACE) Log.i(TAG, "onReceivedError=" + request.url);
            if (mSupportLibClient.isFeatureAvailable(Features.RECEIVE_WEB_RESOURCE_ERROR)) {
                // Note: we must pass AwWebResourceError, since this class was introduced after L.
                mSupportLibClient.onReceivedError(
                        mWebView, new WebResourceRequestAdapter(request), error);
            } else if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
                GlueApiHelperForM.onReceivedError(mWebViewClient, mWebView, request, error);
            }
            // Otherwise, this is handled by {@link #onReceivedError}.
        } finally {
            TraceEvent.end("WebViewContentsClientAdapter.onReceivedError");
        }
    }

    @Override
    public void onSafeBrowsingHit(AwWebResourceRequest request, int threatType,
            final Callback<AwSafeBrowsingResponse> callback) {
        try {
            TraceEvent.begin("WebViewContentsClientAdapter.onSafeBrowsingHit");
            if (mSupportLibClient.isFeatureAvailable(Features.SAFE_BROWSING_HIT)) {
                mSupportLibClient.onSafeBrowsingHit(
                        mWebView, new WebResourceRequestAdapter(request), threatType, callback);
            } else if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O_MR1) {
                GlueApiHelperForOMR1.onSafeBrowsingHit(
                        mWebViewClient, mWebView, request, threatType, callback);

            } else {
                callback.onResult(new AwSafeBrowsingResponse(SafeBrowsingAction.SHOW_INTERSTITIAL,
                        /* reporting */ true));
            }
        } finally {
            TraceEvent.end("WebViewContentsClientAdapter.onSafeBrowsingHit");
        }
    }

    @Override
    public void onReceivedHttpError(AwWebResourceRequest request, AwWebResourceResponse response) {
        try {
            TraceEvent.begin("WebViewContentsClientAdapter.onReceivedHttpError");
            if (TRACE) Log.i(TAG, "onReceivedHttpError=" + request.url);
            if (mSupportLibClient.isFeatureAvailable(Features.RECEIVE_HTTP_ERROR)) {
                String reasonPhrase = response.getReasonPhrase();
                if (reasonPhrase == null || reasonPhrase.isEmpty()) {
                    // We cannot pass a null or empty reasonPhrase, because this version of the
                    // WebResourceResponse constructor will throw. But we may legitimately not
                    // receive a reasonPhrase in the HTTP response, since HTTP/2 removed
                    // Reason-Phrase from the spec (and discourages it). Instead, assign some dummy
                    // value to avoid the crash. See http://crbug.com/925887.
                    reasonPhrase = "UNKNOWN";
                }

                // Note: we do not create an immutable instance here, because that constructor is
                // not available on L.
                mSupportLibClient.onReceivedHttpError(mWebView,
                        new WebResourceRequestAdapter(request),
                        new WebResourceResponse(response.getMimeType(), response.getCharset(),
                                response.getStatusCode(), reasonPhrase,
                                response.getResponseHeaders(), response.getData()));
            } else if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
                GlueApiHelperForM.onReceivedHttpError(mWebViewClient, mWebView, request, response);
            }
            // Otherwise, the API does not exist, so do nothing.
        } finally {
            TraceEvent.end("WebViewContentsClientAdapter.onReceivedHttpError");
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
