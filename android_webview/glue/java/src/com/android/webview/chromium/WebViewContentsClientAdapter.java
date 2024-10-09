// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Picture;
import android.net.Uri;
import android.net.http.SslError;
import android.os.Build;
import android.os.Handler;
import android.os.Message;
import android.view.KeyEvent;
import android.view.View;
import android.view.WindowManager;
import android.webkit.ClientCertRequest;
import android.webkit.ConsoleMessage;
import android.webkit.DownloadListener;
import android.webkit.GeolocationPermissions;
import android.webkit.JsDialogHelper;
import android.webkit.JsPromptResult;
import android.webkit.JsResult;
import android.webkit.PermissionRequest;
import android.webkit.RenderProcessGoneDetail;
import android.webkit.SslErrorHandler;
import android.webkit.ValueCallback;
import android.webkit.WebChromeClient;
import android.webkit.WebResourceResponse;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.webkit.WebViewDelegate;

import org.chromium.android_webview.AwConsoleMessage;
import org.chromium.android_webview.AwContentsClient;
import org.chromium.android_webview.AwContentsClientBridge;
import org.chromium.android_webview.AwGeolocationPermissions;
import org.chromium.android_webview.AwHistogramRecorder;
import org.chromium.android_webview.AwHttpAuthHandler;
import org.chromium.android_webview.AwRenderProcessGoneDetail;
import org.chromium.android_webview.JsPromptResultReceiver;
import org.chromium.android_webview.JsResultReceiver;
import org.chromium.android_webview.R;
import org.chromium.android_webview.common.Lifetime;
import org.chromium.android_webview.permission.AwPermissionRequest;
import org.chromium.android_webview.permission.Resource;
import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.PathUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.ScopedSysTraceEvent;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.components.embedder_support.util.WebResourceResponseInfo;
import org.chromium.content_public.browser.util.DialogTypeRecorder;

import java.lang.ref.WeakReference;
import java.security.Principal;
import java.security.PrivateKey;
import java.security.cert.X509Certificate;
import java.util.ArrayList;
import java.util.WeakHashMap;
import java.util.regex.Pattern;

/**
 * An adapter class that forwards the callbacks from {@link ContentViewClient}
 * to the appropriate {@link WebViewClient} or {@link WebChromeClient}.
 *
 * An instance of this class is associated with one {@link WebViewChromium}
 * instance. A WebViewChromium is a WebView implementation provider (that is
 * android.webkit.WebView delegates all functionality to it) and has exactly
 * one corresponding {@link ContentView} instance.
 *
 * A {@link ContentViewClient} may be shared between multiple {@link ContentView}s,
 * and hence multiple WebViews. Many WebViewClient methods pass the source
 * WebView as an argument. This means that we either need to pass the
 * corresponding ContentView to the corresponding ContentViewClient methods,
 * or use an instance of ContentViewClientAdapter per WebViewChromium, to
 * allow the source WebView to be injected by ContentViewClientAdapter. We
 * choose the latter, because it makes for a cleaner design.
 */
@Lifetime.WebView
class WebViewContentsClientAdapter extends SharedWebViewContentsClientAdapter {
    // The WebChromeClient instance that was passed to WebView.setContentViewClient().
    private WebChromeClient mWebChromeClient;
    // The listener receiving find-in-page API results.
    private WebView.FindListener mFindListener;
    // The listener receiving notifications of screen updates.
    private WebView.PictureListener mPictureListener;
    // Whether the picture listener is invalidate only (i.e. receives a null Picture)
    private boolean mPictureListenerInvalidateOnly;

    private DownloadListener mDownloadListener;

    private Handler mUiThreadHandler;

    private static final int NEW_WEBVIEW_CREATED = 100;

    private WeakHashMap<AwPermissionRequest, WeakReference<PermissionRequestAdapter>>
            mOngoingPermissionRequests;

    // Pattern to match URLs that WebView internally handles as asset or
    // resource lookups.
    private static final Pattern FILE_ANDROID_ASSET_PATTERN =
            Pattern.compile("^file:/*android_(asset|res).*");

    /**
     * Adapter constructor.
     *
     * @param webView the {@link WebView} instance that this adapter is serving.
     */
    @SuppressWarnings("HandlerLeak")
    WebViewContentsClientAdapter(
            WebView webView, Context context, WebViewDelegate webViewDelegate) {
        super(webView, webViewDelegate, context);
        try (ScopedSysTraceEvent event =
                ScopedSysTraceEvent.scoped("WebView.APICallback.WebViewClient.constructor")) {
            // See //android_webview/docs/how-does-on-create-window-work.md for more details.
            mUiThreadHandler =
                    new Handler() {
                        @Override
                        public void handleMessage(Message msg) {
                            switch (msg.what) {
                                case NEW_WEBVIEW_CREATED:
                                    WebView.WebViewTransport t = (WebView.WebViewTransport) msg.obj;
                                    WebView newWebView = t.getWebView();
                                    if (newWebView == mWebView) {
                                        throw new IllegalArgumentException(
                                                "Parent WebView cannot host its own popup window."
                                                        + " Please use"
                                                        + " WebSettings.setSupportMultipleWindows("
                                                        + "false)");
                                    }

                                    if (newWebView != null
                                            && newWebView.copyBackForwardList().getSize() != 0) {
                                        throw new IllegalArgumentException(
                                                "New WebView for popup window must not have been "
                                                        + " previously navigated.");
                                    }

                                    WebViewChromium.completeWindowCreation(mWebView, newWebView);
                                    break;
                                default:
                                    throw new IllegalStateException();
                            }
                        }
                    };
        }
    }

    void setWebChromeClient(WebChromeClient client) {
        mWebChromeClient = client;
    }

    WebChromeClient getWebChromeClient() {
        return mWebChromeClient;
    }

    void setDownloadListener(DownloadListener listener) {
        mDownloadListener = listener;
    }

    void setFindListener(WebView.FindListener listener) {
        mFindListener = listener;
    }

    void setPictureListener(WebView.PictureListener listener, boolean invalidateOnly) {
        mPictureListener = listener;
        mPictureListenerInvalidateOnly = invalidateOnly;
    }

    // --------------------------------------------------------------------------------------------
    //                        Adapter for all the methods.
    // --------------------------------------------------------------------------------------------

    /** @see AwContentsClient#getVisitedHistory. */
    @Override
    public void getVisitedHistory(Callback<String[]> callback) {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICallback.WebViewClient.getVisitedHistory")) {
            AwHistogramRecorder.recordCallbackInvocation(
                    AwHistogramRecorder.WebViewCallbackType.GET_VISITED_HISTORY);
            if (mWebChromeClient != null) {
                if (TRACE) Log.i(TAG, "getVisitedHistory");
                mWebChromeClient.getVisitedHistory(
                        callback == null ? null : value -> callback.onResult(value));
            }
        }
    }

    /** @see AwContentsClient#doUpdateVisiteHistory(String, boolean) */
    @Override
    public void doUpdateVisitedHistory(String url, boolean isReload) {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICallback.WebViewClient.doUpdateVisitedHistory")) {
            AwHistogramRecorder.recordCallbackInvocation(
                    AwHistogramRecorder.WebViewCallbackType.DO_UPDATE_VISITED_HISTORY);
            if (TRACE) Log.i(TAG, "doUpdateVisitedHistory=" + url + " reload=" + isReload);
            mWebViewClient.doUpdateVisitedHistory(mWebView, url, isReload);
        }
    }

    /** @see AwContentsClient#onProgressChanged(int) */
    @Override
    public void onProgressChanged(int progress) {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICallback.WebViewClient.onProgressChanged")) {
            AwHistogramRecorder.recordCallbackInvocation(
                    AwHistogramRecorder.WebViewCallbackType.ON_PROGRESS_CHANGED);
            if (mWebChromeClient != null) {
                if (TRACE) Log.i(TAG, "onProgressChanged=" + progress);
                mWebChromeClient.onProgressChanged(mWebView, progress);
            }
        }
    }

    /** @see AwContentsClient#shouldInterceptRequest(java.lang.String) */
    @Override
    public WebResourceResponseInfo shouldInterceptRequest(AwWebResourceRequest request) {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICallback.WebViewClient.shouldInterceptRequest")) {
            AwHistogramRecorder.recordCallbackInvocation(
                    AwHistogramRecorder.WebViewCallbackType.SHOULD_INTERCEPT_REQUEST);
            if (TRACE) Log.i(TAG, "shouldInterceptRequest=" + request.url);
            WebResourceResponse response =
                    mWebViewClient.shouldInterceptRequest(
                            mWebView, new WebResourceRequestAdapter(request));
            if (response == null) return null;

            return new WebResourceResponseInfo(
                    response.getMimeType(),
                    response.getEncoding(),
                    response.getData(),
                    response.getStatusCode(),
                    response.getReasonPhrase(),
                    response.getResponseHeaders());
        }
    }

    /** @see AwContentsClient#onUnhandledKeyEvent(android.view.KeyEvent) */
    @Override
    public void onUnhandledKeyEvent(KeyEvent event) {
        try (TraceEvent traceEvent =
                TraceEvent.scoped("WebView.APICallback.WebViewClient.onUnhandledKeyEvent")) {
            AwHistogramRecorder.recordCallbackInvocation(
                    AwHistogramRecorder.WebViewCallbackType.ON_UNHANDLED_KEY_EVENT);
            if (TRACE) Log.i(TAG, "onUnhandledKeyEvent");
            mWebViewClient.onUnhandledKeyEvent(mWebView, event);
        }
    }

    /** @see AwContentsClient#onConsoleMessage(android.webkit.ConsoleMessage) */
    @Override
    public boolean onConsoleMessage(AwConsoleMessage consoleMessage) {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICallback.WebViewClient.onConsoleMessage")) {
            AwHistogramRecorder.recordCallbackInvocation(
                    AwHistogramRecorder.WebViewCallbackType.ON_CONSOLE_MESSAGE);
            boolean result;
            if (mWebChromeClient != null) {
                if (TRACE) Log.i(TAG, "onConsoleMessage: " + consoleMessage.message());
                result = mWebChromeClient.onConsoleMessage(fromAwConsoleMessage(consoleMessage));
            } else {
                result = false;
            }
            return result;
        }
    }

    /** @see AwContentsClient#onFindResultReceived(int,int,boolean) */
    @Override
    public void onFindResultReceived(
            int activeMatchOrdinal, int numberOfMatches, boolean isDoneCounting) {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICallback.WebViewClient.onFindResultReceived")) {
            if (mFindListener == null) return;
            if (TRACE) Log.i(TAG, "onFindResultReceived");
            mFindListener.onFindResultReceived(activeMatchOrdinal, numberOfMatches, isDoneCounting);
        }
    }

    /**
     * @see AwContentsClient#onNewPicture(Picture)
     */
    @Override
    public void onNewPicture(Picture picture) {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICallback.WebViewClient.onNewPicture")) {
            if (mPictureListener == null) return;
            if (TRACE) Log.i(TAG, "onNewPicture");
            mPictureListener.onNewPicture(mWebView, picture);
        }
    }

    @Override
    public void onLoadResource(String url) {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICallback.WebViewClient.onLoadResource")) {
            if (TRACE) Log.i(TAG, "onLoadResource=" + url);
            mWebViewClient.onLoadResource(mWebView, url);

            // Record UMA for onLoadResource.
            AwHistogramRecorder.recordCallbackInvocation(
                    AwHistogramRecorder.WebViewCallbackType.ON_LOAD_RESOURCE);
        }
    }

    @Override
    public boolean onCreateWindow(boolean isDialog, boolean isUserGesture) {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICallback.WebViewClient.onCreateWindow")) {
            AwHistogramRecorder.recordCallbackInvocation(
                    AwHistogramRecorder.WebViewCallbackType.ON_CREATE_WINDOW);
            Message m =
                    mUiThreadHandler.obtainMessage(
                            NEW_WEBVIEW_CREATED, mWebView.new WebViewTransport());
            boolean result;
            if (mWebChromeClient != null) {
                if (TRACE) Log.i(TAG, "onCreateWindow");
                result = mWebChromeClient.onCreateWindow(mWebView, isDialog, isUserGesture, m);
            } else {
                result = false;
            }
            return result;
        }
    }

    /** @see AwContentsClient#onCloseWindow() */
    @Override
    public void onCloseWindow() {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICallback.WebViewClient.onCloseWindow")) {
            AwHistogramRecorder.recordCallbackInvocation(
                    AwHistogramRecorder.WebViewCallbackType.ON_CLOSE_WINDOW);
            if (mWebChromeClient != null) {
                if (TRACE) Log.i(TAG, "onCloseWindow");
                mWebChromeClient.onCloseWindow(mWebView);
            }
        }
    }

    /** @see AwContentsClient#onRequestFocus() */
    @Override
    public void onRequestFocus() {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICallback.WebViewClient.onRequestFocus")) {
            AwHistogramRecorder.recordCallbackInvocation(
                    AwHistogramRecorder.WebViewCallbackType.ON_REQUEST_FOCUS);
            if (mWebChromeClient != null) {
                if (TRACE) Log.i(TAG, "onRequestFocus");
                mWebChromeClient.onRequestFocus(mWebView);
            }
        }
    }

    /** @see AwContentsClient#onReceivedTouchIconUrl(String url, boolean precomposed) */
    @Override
    public void onReceivedTouchIconUrl(String url, boolean precomposed) {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICallback.WebViewClient.onReceivedTouchIconUrl")) {
            AwHistogramRecorder.recordCallbackInvocation(
                    AwHistogramRecorder.WebViewCallbackType.ON_RECEIVED_TOUCH_ICON_URL);
            if (mWebChromeClient != null) {
                if (TRACE) Log.i(TAG, "onReceivedTouchIconUrl=" + url);
                mWebChromeClient.onReceivedTouchIconUrl(mWebView, url, precomposed);
            }
        }
    }

    /** @see AwContentsClient#onReceivedIcon(Bitmap bitmap) */
    @Override
    public void onReceivedIcon(Bitmap bitmap) {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICallback.WebViewClient.onReceivedIcon")) {
            AwHistogramRecorder.recordCallbackInvocation(
                    AwHistogramRecorder.WebViewCallbackType.ON_RECEIVED_ICON);
            if (mWebChromeClient != null) {
                if (TRACE) Log.i(TAG, "onReceivedIcon");
                mWebChromeClient.onReceivedIcon(mWebView, bitmap);
            }
        }
    }

    /** @see ContentViewClient#onPageStarted(String) */
    @Override
    public void onPageStarted(String url) {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICallback.WebViewClient.onPageStarted")) {
            if (TRACE) Log.i(TAG, "onPageStarted=" + url);
            mWebViewClient.onPageStarted(mWebView, url, mWebView.getFavicon());

            // Record UMA for onPageStarted.
            AwHistogramRecorder.recordCallbackInvocation(
                    AwHistogramRecorder.WebViewCallbackType.ON_PAGE_STARTED);
        }
    }

    /** @see ContentViewClient#onPageFinished(String) */
    @Override
    public void onPageFinished(String url) {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICallback.WebViewClient.onPageFinished")) {
            if (TRACE) Log.i(TAG, "onPageFinished=" + url);
            mWebViewClient.onPageFinished(mWebView, url);

            // Record UMA for onPageFinished.
            AwHistogramRecorder.recordCallbackInvocation(
                    AwHistogramRecorder.WebViewCallbackType.ON_PAGE_FINISHED);

            // See b/8208948
            // This fakes an onNewPicture callback after onPageFinished to allow
            // CTS tests to run in an un-flaky manner. This is required as the
            // path for sending Picture updates in Chromium are decoupled from the
            // page loading callbacks, i.e. the Chrome compositor may draw our
            // content and send the Picture before onPageStarted or onPageFinished
            // are invoked. The CTS harness discards any pictures it receives before
            // onPageStarted is invoked, so in the case we get the Picture before that and
            // no further updates after onPageStarted, we'll fail the test by timing
            // out waiting for a Picture.
            if (mPictureListener != null) {
                PostTask.postDelayedTask(
                        TaskTraits.UI_DEFAULT,
                        () -> {
                            if (mPictureListener != null) {
                                if (TRACE) {
                                    Log.i(TAG, "onNewPicture - from onPageFinished workaround.");
                                }
                                mPictureListener.onNewPicture(
                                        mWebView,
                                        mPictureListenerInvalidateOnly ? null : new Picture());
                            }
                        },
                        100);
            }
        }
    }

    /** @see ContentViewClient#onReceivedTitle(String) */
    @Override
    public void onReceivedTitle(String title) {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICallback.WebViewClient.onReceivedTitle")) {
            AwHistogramRecorder.recordCallbackInvocation(
                    AwHistogramRecorder.WebViewCallbackType.ON_RECEIVED_TITLE);
            if (mWebChromeClient != null) {
                if (TRACE) Log.i(TAG, "onReceivedTitle=\"" + title + "\"");
                mWebChromeClient.onReceivedTitle(mWebView, title);
            }
        }
    }

    /** @see ContentViewClient#shouldOverrideKeyEvent(KeyEvent) */
    @Override
    public boolean shouldOverrideKeyEvent(KeyEvent event) {
        try (TraceEvent traceEvent =
                TraceEvent.scoped("WebView.APICallback.WebViewClient.shouldOverrideKeyEvent")) {
            AwHistogramRecorder.recordCallbackInvocation(
                    AwHistogramRecorder.WebViewCallbackType.SHOULD_OVERRIDE_KEY_EVENT);
            if (TRACE) Log.i(TAG, "shouldOverrideKeyEvent");
            return mWebViewClient.shouldOverrideKeyEvent(mWebView, event);
        }
    }

    /**
     * Returns true if a method with a given name and parameters is declared in a subclass
     * of a given baseclass.
     */
    private static <T> boolean isMethodDeclaredInSubClass(
            Class<T> baseClass,
            Class<? extends T> subClass,
            String name,
            Class<?>... parameterTypes) {
        try {
            return !subClass.getMethod(name, parameterTypes).getDeclaringClass().equals(baseClass);
        } catch (SecurityException e) {
            return false;
        } catch (NoSuchMethodException e) {
            return false;
        }
    }

    @Override
    public void onGeolocationPermissionsShowPrompt(
            String origin, AwGeolocationPermissions.Callback callback) {
        try (TraceEvent traceEvent =
                TraceEvent.scoped(
                        "WebView.APICallback.WebViewClient.onGeolocationPermissionsShowPrompt")) {
            AwHistogramRecorder.recordCallbackInvocation(
                    AwHistogramRecorder.WebViewCallbackType.ON_GEOLOCATION_PERMISSIONS_SHOW_PROMPT);
            if (mWebChromeClient == null) {
                callback.invoke(origin, false, false);
                return;
            }
            if (!isMethodDeclaredInSubClass(
                    WebChromeClient.class,
                    mWebChromeClient.getClass(),
                    "onGeolocationPermissionsShowPrompt",
                    String.class,
                    GeolocationPermissions.Callback.class)) {
                // The default WebChromeClient.onGeolocationPermissionsShowPrompt() implementation
                // is a NOOP (does not invoke the callback). Explicitly invoke the callback in
                // chromium code to deny the permission.
                callback.invoke(origin, false, false);
                return;
            }
            if (TRACE) Log.i(TAG, "onGeolocationPermissionsShowPrompt");
            final long requestStartTime = System.currentTimeMillis();
            GeolocationPermissions.Callback callbackWrapper =
                    (callbackOrigin, allow, retain) -> {
                        RecordHistogram.recordTimesHistogram(
                                "Android.WebView.OnGeolocationPermissionsShowPrompt.ResponseTime",
                                System.currentTimeMillis() - requestStartTime);
                        RecordHistogram.recordBooleanHistogram(
                                "Android.WebView.OnGeolocationPermissionsShowPrompt.Allow", allow);
                        RecordHistogram.recordBooleanHistogram(
                                "Android.WebView.OnGeolocationPermissionsShowPrompt.Retain",
                                retain);
                        callback.invoke(callbackOrigin, allow, retain);
                    };
            mWebChromeClient.onGeolocationPermissionsShowPrompt(
                    origin, callback == null ? null : callbackWrapper);
        }
    }

    @Override
    public void onGeolocationPermissionsHidePrompt() {
        try (TraceEvent event =
                TraceEvent.scoped(
                        "WebView.APICallback.WebViewClient.onGeolocationPermissionsHidePrompt")) {
            AwHistogramRecorder.recordCallbackInvocation(
                    AwHistogramRecorder.WebViewCallbackType.ON_GEOLOCATION_PERMISSIONS_HIDE_PROMPT);
            if (mWebChromeClient != null) {
                if (TRACE) Log.i(TAG, "onGeolocationPermissionsHidePrompt");
                mWebChromeClient.onGeolocationPermissionsHidePrompt();
            }
        }
    }

    @Override
    public void onPermissionRequest(AwPermissionRequest permissionRequest) {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICallback.WebViewClient.onPermissionRequest")) {
            AwHistogramRecorder.recordCallbackInvocation(
                    AwHistogramRecorder.WebViewCallbackType.ON_PERMISSION_REQUEST);
            if (mWebChromeClient != null) {
                if (TRACE) Log.i(TAG, "onPermissionRequest");
                if (mOngoingPermissionRequests == null) {
                    mOngoingPermissionRequests = new WeakHashMap<>();
                }
                PermissionRequestAdapter adapter = new PermissionRequestAdapter(permissionRequest);
                mOngoingPermissionRequests.put(permissionRequest, new WeakReference<>(adapter));
                mWebChromeClient.onPermissionRequest(adapter);
            } else {
                // By default, we deny the permission.
                permissionRequest.deny();
            }
        }
    }

    @Override
    public void onPermissionRequestCanceled(AwPermissionRequest permissionRequest) {
        try (TraceEvent event =
                TraceEvent.scoped(
                        "WebView.APICallback.WebViewClient.onPermissionRequestCanceled")) {
            AwHistogramRecorder.recordCallbackInvocation(
                    AwHistogramRecorder.WebViewCallbackType.ON_PERMISSION_REQUEST_CANCELED);
            if (mWebChromeClient != null && mOngoingPermissionRequests != null) {
                if (TRACE) Log.i(TAG, "onPermissionRequestCanceled");
                WeakReference<PermissionRequestAdapter> weakRef =
                        mOngoingPermissionRequests.get(permissionRequest);
                // We don't hold strong reference to PermissionRequestAdpater and don't expect the
                // user only holds weak reference to it either, if so, user has no way to call
                // grant()/deny(), and no need to be notified the cancellation of request.
                if (weakRef != null) {
                    PermissionRequestAdapter adapter = weakRef.get();
                    if (adapter != null) mWebChromeClient.onPermissionRequestCanceled(adapter);
                }
            }
        }
    }

    private static class JsPromptResultReceiverAdapter implements JsResult.ResultReceiver {
        private JsPromptResultReceiver mChromePromptResultReceiver;
        private JsResultReceiver mChromeResultReceiver;
        // We hold onto the JsPromptResult here, just to avoid the need to downcast
        // in onJsResultComplete.
        private final JsPromptResult mPromptResult = new JsPromptResult(this);

        public JsPromptResultReceiverAdapter(JsPromptResultReceiver receiver) {
            mChromePromptResultReceiver = receiver;
        }

        public JsPromptResultReceiverAdapter(JsResultReceiver receiver) {
            mChromeResultReceiver = receiver;
        }

        public JsPromptResult getPromptResult() {
            return mPromptResult;
        }

        @Override
        public void onJsResultComplete(JsResult result) {
            if (mChromePromptResultReceiver != null) {
                if (mPromptResult.getResult()) {
                    mChromePromptResultReceiver.confirm(mPromptResult.getStringResult());
                } else {
                    mChromePromptResultReceiver.cancel();
                }
            } else {
                if (mPromptResult.getResult()) {
                    mChromeResultReceiver.confirm();
                } else {
                    mChromeResultReceiver.cancel();
                }
            }
        }
    }

    @Override
    public void handleJsAlert(String url, String message, JsResultReceiver receiver) {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICallback.WebViewClient.handleJsAlert")) {
            AwHistogramRecorder.recordCallbackInvocation(
                    AwHistogramRecorder.WebViewCallbackType.ON_JS_ALERT);
            if (mWebChromeClient != null) {
                final JsPromptResult res =
                        new JsPromptResultReceiverAdapter(receiver).getPromptResult();
                if (TRACE) Log.i(TAG, "onJsAlert");
                if (!mWebChromeClient.onJsAlert(mWebView, url, message, res)) {
                    if (!showDefaultJsDialog(res, JsDialogHelper.ALERT, null, message, url)) {
                        receiver.cancel();
                    }
                }
            } else {
                receiver.cancel();
            }
        }
    }

    @Override
    public void handleJsBeforeUnload(String url, String message, JsResultReceiver receiver) {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICallback.WebViewClient.handleJsBeforeUnload")) {
            AwHistogramRecorder.recordCallbackInvocation(
                    AwHistogramRecorder.WebViewCallbackType.ON_JS_BEFORE_UNLOAD);
            if (mWebChromeClient != null) {
                final JsPromptResult res =
                        new JsPromptResultReceiverAdapter(receiver).getPromptResult();
                if (TRACE) Log.i(TAG, "onJsBeforeUnload");
                if (!mWebChromeClient.onJsBeforeUnload(mWebView, url, message, res)) {
                    if (!showDefaultJsDialog(res, JsDialogHelper.UNLOAD, null, message, url)) {
                        receiver.cancel();
                    }
                }
            } else {
                receiver.cancel();
            }
        }
    }

    @Override
    public void handleJsConfirm(String url, String message, JsResultReceiver receiver) {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICallback.WebViewClient.handleJsConfirm")) {
            AwHistogramRecorder.recordCallbackInvocation(
                    AwHistogramRecorder.WebViewCallbackType.ON_JS_CONFIRM);
            if (mWebChromeClient != null) {
                final JsPromptResult res =
                        new JsPromptResultReceiverAdapter(receiver).getPromptResult();
                if (TRACE) Log.i(TAG, "onJsConfirm");
                if (!mWebChromeClient.onJsConfirm(mWebView, url, message, res)) {
                    if (!showDefaultJsDialog(res, JsDialogHelper.CONFIRM, null, message, url)) {
                        receiver.cancel();
                    }
                }
            } else {
                receiver.cancel();
            }
        }
    }

    @Override
    public void handleJsPrompt(
            String url, String message, String defaultValue, JsPromptResultReceiver receiver) {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICallback.WebViewClient.handleJsPrompt")) {
            AwHistogramRecorder.recordCallbackInvocation(
                    AwHistogramRecorder.WebViewCallbackType.ON_JS_PROMPT);
            if (mWebChromeClient != null) {
                final JsPromptResult res =
                        new JsPromptResultReceiverAdapter(receiver).getPromptResult();
                if (TRACE) Log.i(TAG, "onJsPrompt");
                if (!mWebChromeClient.onJsPrompt(mWebView, url, message, defaultValue, res)) {
                    if (!showDefaultJsDialog(
                            res, JsDialogHelper.PROMPT, defaultValue, message, url)) {
                        receiver.cancel();
                    }
                }
            } else {
                receiver.cancel();
            }
        }
    }

    /** Try to show the default JS dialog and return whether the dialog was shown. */
    private boolean showDefaultJsDialog(
            JsPromptResult res, int jsDialogType, String defaultValue, String message, String url) {
        // Note we must unwrap the Context here due to JsDialogHelper only using instanceof to
        // check if a Context is an Activity.
        Context activityContext = ContextUtils.activityFromContext(mContext);
        if (activityContext == null) {
            Log.w(TAG, "Unable to create JsDialog without an Activity");
            return false;
        }
        try {
            new JsDialogHelper(res, jsDialogType, defaultValue, message, url)
                    .showDialog(activityContext);
            DialogTypeRecorder.recordDialogType(DialogTypeRecorder.DialogType.JS_POPUP);
        } catch (WindowManager.BadTokenException e) {
            Log.w(
                    TAG,
                    "Unable to create JsDialog. Has this WebView outlived the Activity it was"
                            + " created with?");
            return false;
        }
        return true;
    }

    @Override
    public void onReceivedHttpAuthRequest(AwHttpAuthHandler handler, String host, String realm) {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICallback.WebViewClient.onReceivedHttpAuthRequest")) {
            AwHistogramRecorder.recordCallbackInvocation(
                    AwHistogramRecorder.WebViewCallbackType.ON_RECEIVED_HTTP_AUTH_REQUEST);
            if (TRACE) Log.i(TAG, "onReceivedHttpAuthRequest=" + host);
            mWebViewClient.onReceivedHttpAuthRequest(
                    mWebView, new AwHttpAuthHandlerAdapter(handler), host, realm);
        }
    }

    @Override
    @SuppressWarnings("HandlerLeak")
    public void onReceivedSslError(final Callback<Boolean> callback, SslError error) {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICallback.WebViewClient.onReceivedSslError")) {
            AwHistogramRecorder.recordCallbackInvocation(
                    AwHistogramRecorder.WebViewCallbackType.ON_RECEIVED_SSL_ERROR);
            SslErrorHandler handler =
                    new SslErrorHandler() {
                        @Override
                        public void proceed() {
                            callback.onResult(true);
                        }

                        @Override
                        public void cancel() {
                            callback.onResult(false);
                        }
                    };
            if (TRACE) Log.i(TAG, "onReceivedSslError");
            mWebViewClient.onReceivedSslError(mWebView, handler, error);
        }
    }

    private static class ClientCertRequestImpl extends ClientCertRequest {
        private final AwContentsClientBridge.ClientCertificateRequestCallback mCallback;
        private final String[] mKeyTypes;
        private final Principal[] mPrincipals;
        private final String mHost;
        private final int mPort;

        public ClientCertRequestImpl(
                AwContentsClientBridge.ClientCertificateRequestCallback callback,
                String[] keyTypes,
                Principal[] principals,
                String host,
                int port) {
            mCallback = callback;
            mKeyTypes = keyTypes;
            mPrincipals = principals;
            mHost = host;
            mPort = port;
        }

        @Override
        public String[] getKeyTypes() {
            // This is already a copy of native argument, so return directly.
            return mKeyTypes;
        }

        @Override
        public Principal[] getPrincipals() {
            // This is already a copy of native argument, so return directly.
            return mPrincipals;
        }

        @Override
        public String getHost() {
            return mHost;
        }

        @Override
        public int getPort() {
            return mPort;
        }

        @Override
        public void proceed(final PrivateKey privateKey, final X509Certificate[] chain) {
            mCallback.proceed(privateKey, chain);
        }

        @Override
        public void ignore() {
            mCallback.ignore();
        }

        @Override
        public void cancel() {
            mCallback.cancel();
        }
    }

    @Override
    public void onReceivedClientCertRequest(
            AwContentsClientBridge.ClientCertificateRequestCallback callback,
            String[] keyTypes,
            Principal[] principals,
            String host,
            int port) {
        if (TRACE) Log.i(TAG, "onReceivedClientCertRequest");
        try (TraceEvent event =
                TraceEvent.scoped(
                        "WebView.APICallback.WebViewClient.onReceivedClientCertRequest")) {
            AwHistogramRecorder.recordCallbackInvocation(
                    AwHistogramRecorder.WebViewCallbackType.ON_RECEIVED_CLIENT_CERT_REQUEST);
            final ClientCertRequestImpl request =
                    new ClientCertRequestImpl(callback, keyTypes, principals, host, port);
            mWebViewClient.onReceivedClientCertRequest(mWebView, request);
        }
    }

    @Override
    public void onReceivedLoginRequest(String realm, String account, String args) {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICallback.WebViewClient.onReceivedLoginRequest")) {
            AwHistogramRecorder.recordCallbackInvocation(
                    AwHistogramRecorder.WebViewCallbackType.ON_RECEIVED_LOGIN_REQUEST);
            if (TRACE) Log.i(TAG, "onReceivedLoginRequest=" + realm);
            mWebViewClient.onReceivedLoginRequest(mWebView, realm, account, args);
        }
    }

    @Override
    public void onFormResubmission(Message dontResend, Message resend) {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICallback.WebViewClient.onFormResubmission")) {
            AwHistogramRecorder.recordCallbackInvocation(
                    AwHistogramRecorder.WebViewCallbackType.ON_FORM_RESUBMISSION);
            if (TRACE) Log.i(TAG, "onFormResubmission");
            mWebViewClient.onFormResubmission(mWebView, dontResend, resend);
        }
    }

    @Override
    public void onDownloadStart(
            String url,
            String userAgent,
            String contentDisposition,
            String mimeType,
            long contentLength) {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICallback.WebViewClient.onDownloadStart")) {
            if (mDownloadListener != null) {
                if (TRACE) Log.i(TAG, "onDownloadStart");
                mDownloadListener.onDownloadStart(
                        url, userAgent, contentDisposition, mimeType, contentLength);
            }
        }
    }

    @Override
    public void showFileChooser(
            final Callback<String[]> uploadFileCallback,
            final AwContentsClient.FileChooserParamsImpl fileChooserParams) {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICallback.WebViewClient.showFileChooser")) {
            if (mWebChromeClient == null) {
                uploadFileCallback.onResult(null);
                return;
            }
            if (TRACE) Log.i(TAG, "showFileChooser");
            ValueCallback<Uri[]> callbackAdapter =
                    new ValueCallback<Uri[]>() {
                        private boolean mCompleted;

                        @Override
                        public void onReceiveValue(Uri[] uriList) {
                            if (mCompleted) {
                                throw new IllegalStateException(
                                        "showFileChooser result was already called");
                            }
                            mCompleted = true;
                            String[] s = null;
                            if (uriList != null) {
                                s = new String[uriList.length];
                                for (int i = 0; i < uriList.length; i++) {
                                    s[i] = uriList[i].toString();
                                    if ("file".equals(uriList[i].getScheme())
                                            && !FILE_ANDROID_ASSET_PATTERN
                                                    .matcher(s[i])
                                                    .matches()) {
                                        RecordHistogram.recordBooleanHistogram(
                                                "Android.WebView.FileChooserResultOutsideAppDataDir",
                                                PathUtils.isPathUnderAppDir(
                                                        uriList[i].getSchemeSpecificPart(),
                                                        mContext));
                                    }
                                }
                            }
                            uploadFileCallback.onResult(s);
                        }
                    };

            // Invoke the new callback introduced in Lollipop. If the app handles
            // it, we're done here.
            if (mWebChromeClient.onShowFileChooser(
                    mWebView, callbackAdapter, fromAwFileChooserParams(fileChooserParams))) {
                return;
            }

            // If the app did not handle it and we are running on Lollipop or newer, then
            // abort.
            if (mContext.getApplicationInfo().targetSdkVersion >= Build.VERSION_CODES.LOLLIPOP) {
                uploadFileCallback.onResult(null);
                return;
            }

            // Otherwise, for older apps, attempt to invoke the legacy (hidden) API for
            // backwards compatibility.
            ValueCallback<Uri> innerCallback =
                    new ValueCallback<Uri>() {
                        private boolean mCompleted;

                        @Override
                        public void onReceiveValue(Uri uri) {
                            if (mCompleted) {
                                throw new IllegalStateException(
                                        "showFileChooser result was already called");
                            }
                            mCompleted = true;
                            uploadFileCallback.onResult(
                                    uri == null ? null : new String[] {uri.toString()});
                        }
                    };
            if (TRACE) Log.i(TAG, "openFileChooser");
            mWebChromeClient.openFileChooser(
                    innerCallback,
                    fileChooserParams.getAcceptTypesString(),
                    fileChooserParams.isCaptureEnabled() ? "*" : "");
        }
    }

    @Override
    public void onScaleChangedScaled(float oldScale, float newScale) {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICallback.WebViewClient.onScaleChangedScaled")) {
            AwHistogramRecorder.recordCallbackInvocation(
                    AwHistogramRecorder.WebViewCallbackType.ON_SCALE_CHANGED);
            if (TRACE) Log.i(TAG, " onScaleChangedScaled");
            mWebViewClient.onScaleChanged(mWebView, oldScale, newScale);
        }
    }

    @Override
    public void onShowCustomView(View view, final CustomViewCallback cb) {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICallback.WebViewClient.onShowCustomView")) {
            AwHistogramRecorder.recordCallbackInvocation(
                    AwHistogramRecorder.WebViewCallbackType.ON_SHOW_CUSTOM_VIEW);
            if (mWebChromeClient != null) {
                if (TRACE) Log.i(TAG, "onShowCustomView");
                mWebChromeClient.onShowCustomView(
                        view, cb == null ? null : () -> cb.onCustomViewHidden());
            }
        }
    }

    @Override
    public void onHideCustomView() {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICallback.WebViewClient.onHideCustomView")) {
            AwHistogramRecorder.recordCallbackInvocation(
                    AwHistogramRecorder.WebViewCallbackType.ON_HIDE_CUSTOM_VIEW);
            if (mWebChromeClient != null) {
                if (TRACE) Log.i(TAG, "onHideCustomView");
                mWebChromeClient.onHideCustomView();
            }
        }
    }

    @Override
    protected View getVideoLoadingProgressView() {
        try (TraceEvent event =
                TraceEvent.scoped(
                        "WebView.APICallback.WebViewClient.getVideoLoadingProgressView")) {
            View result;
            if (mWebChromeClient != null) {
                if (TRACE) Log.i(TAG, "getVideoLoadingProgressView");
                result = mWebChromeClient.getVideoLoadingProgressView();
            } else {
                result = null;
            }
            return result;
        }
    }

    @Override
    public Bitmap getDefaultVideoPoster() {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICallback.WebViewClient.getDefaultVideoPoster")) {
            AwHistogramRecorder.recordCallbackInvocation(
                    AwHistogramRecorder.WebViewCallbackType.GET_DEFAULT_VIDEO_POSTER);
            Bitmap result = null;
            if (mWebChromeClient != null) {
                if (TRACE) Log.i(TAG, "getDefaultVideoPoster");
                result = mWebChromeClient.getDefaultVideoPoster();
            }
            if (result == null) {
                Bitmap poster =
                        BitmapFactory.decodeResource(
                                mContext.getResources(),
                                R.drawable.ic_play_circle_outline_black_48dp);

                // WebView relies on the application's resources from the context we have.
                // If the application does anything to change how these resources work,
                // this could result in us failing to retrieve the bitmap.
                // It is not a fix, and we could still run into other problems, but we
                // will fall back to an empty Bitmap rather than try use the resource we
                // couldn't retrieve to try to help apps that may run into this problem.
                // See crbug.com/329106309 for more information.
                if (poster != null) {
                    // The ic_play_circle_outline_black_48dp icon is transparent so we need to draw
                    // it on a gray background.
                    result =
                            Bitmap.createBitmap(
                                    poster.getWidth(), poster.getHeight(), poster.getConfig());
                    result.eraseColor(Color.GRAY);
                    Canvas canvas = new Canvas(result);
                    canvas.drawBitmap(poster, 0f, 0f, null);
                } else {
                    Log.w(TAG, "Unable to retrieve default video poster from resources");
                    result =
                            Bitmap.createBitmap(
                                    new int[] {Color.TRANSPARENT}, 1, 1, Bitmap.Config.ARGB_8888);
                }
            }
            return result;
        }
    }

    @Override
    public boolean onRenderProcessGone(final AwRenderProcessGoneDetail detail) {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICallback.WebViewClient.onRenderProcessGone")) {
            AwHistogramRecorder.recordCallbackInvocation(
                    AwHistogramRecorder.WebViewCallbackType.ON_RENDER_PROCESS_GONE);
            return mWebViewClient.onRenderProcessGone(
                    mWebView,
                    new RenderProcessGoneDetail() {
                        @Override
                        public boolean didCrash() {
                            return detail.didCrash();
                        }

                        @Override
                        @SuppressWarnings("WrongConstant") // https://crbug.com/1509716
                        public int rendererPriorityAtExit() {
                            return detail.rendererPriority();
                        }
                    });
        }
    }

    private static class AwHttpAuthHandlerAdapter extends android.webkit.HttpAuthHandler {
        private AwHttpAuthHandler mAwHandler;

        public AwHttpAuthHandlerAdapter(AwHttpAuthHandler awHandler) {
            mAwHandler = awHandler;
        }

        @Override
        public void proceed(String username, String password) {
            if (username == null) {
                username = "";
            }

            if (password == null) {
                password = "";
            }
            mAwHandler.proceed(username, password);
        }

        @Override
        public void cancel() {
            mAwHandler.cancel();
        }

        @Override
        public boolean useHttpAuthUsernamePassword() {
            return mAwHandler.isFirstAttempt();
        }
    }

    /** Type adaptation class for PermissionRequest. */
    public static class PermissionRequestAdapter extends PermissionRequest {

        private static long toAwPermissionResources(String[] resources) {
            long result = 0;
            for (String resource : resources) {
                if (resource.equals(PermissionRequest.RESOURCE_VIDEO_CAPTURE)) {
                    result |= Resource.VIDEO_CAPTURE;
                } else if (resource.equals(PermissionRequest.RESOURCE_AUDIO_CAPTURE)) {
                    result |= Resource.AUDIO_CAPTURE;
                } else if (resource.equals(PermissionRequest.RESOURCE_PROTECTED_MEDIA_ID)) {
                    result |= Resource.PROTECTED_MEDIA_ID;
                } else if (resource.equals(PermissionRequest.RESOURCE_MIDI_SYSEX)) {
                    result |= Resource.MIDI_SYSEX;
                }
            }
            return result;
        }

        private static String[] toPermissionResources(long resources) {
            ArrayList<String> result = new ArrayList<String>();
            if ((resources & Resource.VIDEO_CAPTURE) != 0) {
                result.add(PermissionRequest.RESOURCE_VIDEO_CAPTURE);
            }
            if ((resources & Resource.AUDIO_CAPTURE) != 0) {
                result.add(PermissionRequest.RESOURCE_AUDIO_CAPTURE);
            }
            if ((resources & Resource.PROTECTED_MEDIA_ID) != 0) {
                result.add(PermissionRequest.RESOURCE_PROTECTED_MEDIA_ID);
            }
            if ((resources & Resource.MIDI_SYSEX) != 0) {
                result.add(PermissionRequest.RESOURCE_MIDI_SYSEX);
            }
            String[] resource_array = new String[result.size()];
            return result.toArray(resource_array);
        }

        private AwPermissionRequest mAwPermissionRequest;
        private final String[] mResources;

        private final long mCreationTime;

        public PermissionRequestAdapter(AwPermissionRequest awPermissionRequest) {
            assert awPermissionRequest != null;
            mAwPermissionRequest = awPermissionRequest;
            mResources = toPermissionResources(mAwPermissionRequest.getResources());
            mCreationTime = System.currentTimeMillis();
            RecordHistogram.recordCount100Histogram(
                    "Android.WebView.OnPermissionRequest.RequestedResourceCount",
                    mResources.length);
            // The resources result is a bitmask of size 2^5 (32 distinct values).
            RecordHistogram.recordSparseHistogram(
                    "Android.WebView.OnPermissionRequest.RequestedResources",
                    (int) mAwPermissionRequest.getResources());
        }

        @Override
        public Uri getOrigin() {
            return mAwPermissionRequest.getOrigin();
        }

        @Override
        public String[] getResources() {
            return mResources.clone();
        }

        @Override
        public void grant(String[] resources) {
            recordResponseTime();
            long requestedResource = mAwPermissionRequest.getResources();
            if ((requestedResource & toAwPermissionResources(resources)) == requestedResource) {
                recordPermissionResult(true);
                mAwPermissionRequest.grant();
            } else {
                recordPermissionResult(false);
                mAwPermissionRequest.deny();
            }
        }

        @Override
        public void deny() {
            recordResponseTime();
            recordPermissionResult(false);
            mAwPermissionRequest.deny();
        }

        private void recordPermissionResult(boolean granted) {
            RecordHistogram.recordBooleanHistogram(
                    "Android.WebView.OnPermissionRequest.Granted", granted);
        }

        /** Record the response time from the app to a histogram. */
        private void recordResponseTime() {
            long duration = System.currentTimeMillis() - mCreationTime;
            RecordHistogram.recordTimesHistogram(
                    "Android.WebView.OnPermissionRequest.ResponseTime", duration);
        }
    }

    public static WebChromeClient.FileChooserParams fromAwFileChooserParams(
            final AwContentsClient.FileChooserParamsImpl value) {
        if (value == null) {
            return null;
        }
        return new WebChromeClient.FileChooserParams() {
            @Override
            public int getMode() {
                return value.getMode();
            }

            @Override
            public String[] getAcceptTypes() {
                return value.getAcceptTypes();
            }

            @Override
            public boolean isCaptureEnabled() {
                return value.isCaptureEnabled();
            }

            @Override
            public CharSequence getTitle() {
                return value.getTitle();
            }

            @Override
            public String getFilenameHint() {
                return value.getFilenameHint();
            }

            @Override
            public Intent createIntent() {
                return value.createIntent();
            }
        };
    }

    private static ConsoleMessage fromAwConsoleMessage(AwConsoleMessage value) {
        if (value == null) {
            return null;
        }
        return new ConsoleMessage(
                value.message(),
                value.sourceId(),
                value.lineNumber(),
                fromAwMessageLevel(value.messageLevel()));
    }

    private static ConsoleMessage.MessageLevel fromAwMessageLevel(
            @AwConsoleMessage.MessageLevel int value) {
        switch (value) {
            case AwConsoleMessage.MESSAGE_LEVEL_TIP:
                return ConsoleMessage.MessageLevel.TIP;
            case AwConsoleMessage.MESSAGE_LEVEL_LOG:
                return ConsoleMessage.MessageLevel.LOG;
            case AwConsoleMessage.MESSAGE_LEVEL_WARNING:
                return ConsoleMessage.MessageLevel.WARNING;
            case AwConsoleMessage.MESSAGE_LEVEL_ERROR:
                return ConsoleMessage.MessageLevel.ERROR;
            case AwConsoleMessage.MESSAGE_LEVEL_DEBUG:
                return ConsoleMessage.MessageLevel.DEBUG;
            default:
                throw new IllegalArgumentException("Unsupported value: " + value);
        }
    }
}
