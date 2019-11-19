// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.app.Activity;
import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.Picture;
import android.net.Uri;
import android.net.http.SslError;
import android.os.Looper;
import android.os.Message;
import android.provider.Browser;
import android.text.TextUtils;
import android.view.KeyEvent;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.android_webview.permission.AwPermissionRequest;
import org.chromium.android_webview.safe_browsing.AwSafeBrowsingResponse;
import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.ScopedSysTraceEvent;
import org.chromium.content_public.common.ContentUrlConstants;

import java.security.Principal;
import java.util.HashMap;

/**
 * Base-class that an AwContents embedder derives from to receive callbacks.
 * For any other callbacks we need to make transformations of (e.g. adapt parameters
 * or perform filtering) we can provide final overrides for methods here, and then introduce
 * new abstract methods that the our own client must implement.
 * i.e.: all methods in this class should either be final, or abstract.
 */
public abstract class AwContentsClient {
    private static final String TAG = "AwContentsClient";
    private final AwContentsClientCallbackHelper mCallbackHelper;

    // Last background color reported from the renderer. Holds the sentinal value INVALID_COLOR
    // if not valid.
    private int mCachedRendererBackgroundColor = INVALID_COLOR;
    // Holds the last known page title. {@link ContentViewClient#onUpdateTitle} is unreliable,
    // particularly for navigating backwards and forwards in the history stack. Instead, the last
    // known document title is kept here, and the clients gets updated whenever the value has
    // actually changed. Blink also only sends updates when the document title have changed,
    // so behaviours are consistent.
    private String mTitle = "";

    private static final int INVALID_COLOR = 0;

    public AwContentsClient() {
        this(Looper.myLooper());
    }

    /**
     *
     * See {@link android.webkit.WebChromeClient}. */
    public interface CustomViewCallback {
        /* See {@link android.webkit.WebChromeClient}. */
        public void onCustomViewHidden();
    }

    // Alllow injection of the callback thread, for testing.
    public AwContentsClient(Looper looper) {
        try (ScopedSysTraceEvent e =
                        ScopedSysTraceEvent.scoped("AwContentsClient.constructorOneArg")) {
            mCallbackHelper = new AwContentsClientCallbackHelper(looper, this);
        }
    }

    final AwContentsClientCallbackHelper getCallbackHelper() {
        return mCallbackHelper;
    }

    final int getCachedRendererBackgroundColor() {
        assert isCachedRendererBackgroundColorValid();
        return mCachedRendererBackgroundColor;
    }

    final boolean isCachedRendererBackgroundColorValid() {
        return mCachedRendererBackgroundColor != INVALID_COLOR;
    }

    final void onBackgroundColorChanged(int color) {
        // Avoid storing the sentinal INVALID_COLOR (note that both 0 and 1 are both
        // fully transparent so this transpose makes no visible difference).
        mCachedRendererBackgroundColor = color == INVALID_COLOR ? 1 : color;
    }

    //--------------------------------------------------------------------------------------------
    //             WebView specific methods that map directly to WebViewClient / WebChromeClient
    //--------------------------------------------------------------------------------------------

    /**
     * Parameters for the {@link AwContentsClient#shouldInterceptRequest} method.
     */
    public static class AwWebResourceRequest {
        // Prefer using other constructors over this one.
        public AwWebResourceRequest() {}

        public AwWebResourceRequest(String url, boolean isMainFrame, boolean hasUserGesture,
                String method, @Nullable HashMap<String, String> requestHeaders) {
            this.url = url;
            this.isMainFrame = isMainFrame;
            this.hasUserGesture = hasUserGesture;
            // Note: we intentionally let isRedirect default initialize to false. This is because we
            // don't always know if this request is associated with a redirect or not.
            this.method = method;
            this.requestHeaders = requestHeaders;
        }

        public AwWebResourceRequest(String url, boolean isMainFrame, boolean hasUserGesture,
                String method, @NonNull String[] requestHeaderNames,
                @NonNull String[] requestHeaderValues) {
            this(url, isMainFrame, hasUserGesture, method,
                    new HashMap<String, String>(requestHeaderValues.length));
            for (int i = 0; i < requestHeaderNames.length; ++i) {
                this.requestHeaders.put(requestHeaderNames[i], requestHeaderValues[i]);
            }
        }

        // Url of the request.
        public String url;
        // Is this for the main frame or a child iframe?
        public boolean isMainFrame;
        // Was a gesture associated with the request? Don't trust can easily be spoofed.
        public boolean hasUserGesture;
        // Was it a result of a server-side redirect?
        public boolean isRedirect;
        // Method used (GET/POST/OPTIONS)
        public String method;
        // Headers that would have been sent to server.
        public HashMap<String, String> requestHeaders;
    }

    /**
     * Parameters for {@link AwContentsClient#onReceivedError} method.
     */
    public static class AwWebResourceError {
        public int errorCode = ErrorCodeConversionHelper.ERROR_UNKNOWN;
        public String description;
    }

    /**
     * Allow default implementations in chromium code.
     */
    public abstract boolean hasWebViewClient();

    public abstract void getVisitedHistory(Callback<String[]> callback);

    public abstract void doUpdateVisitedHistory(String url, boolean isReload);

    public abstract void onProgressChanged(int progress);

    public abstract AwWebResourceResponse shouldInterceptRequest(
            AwWebResourceRequest request);

    public abstract boolean shouldOverrideKeyEvent(KeyEvent event);

    public abstract boolean shouldOverrideUrlLoading(AwWebResourceRequest request);

    public abstract void onLoadResource(String url);

    public abstract void onUnhandledKeyEvent(KeyEvent event);

    public abstract boolean onConsoleMessage(AwConsoleMessage consoleMessage);

    public abstract void onReceivedHttpAuthRequest(AwHttpAuthHandler handler,
            String host, String realm);

    public abstract void onReceivedSslError(Callback<Boolean> callback, SslError error);

    public abstract void onReceivedClientCertRequest(
            final AwContentsClientBridge.ClientCertificateRequestCallback callback,
            final String[] keyTypes, final Principal[] principals, final String host,
            final int port);

    public abstract void onReceivedLoginRequest(String realm, String account, String args);

    public abstract void onFormResubmission(Message dontResend, Message resend);

    public abstract void onDownloadStart(String url, String userAgent, String contentDisposition,
            String mimeType, long contentLength);

    public final boolean shouldIgnoreNavigation(Context context, String url, boolean isMainFrame,
            boolean hasUserGesture, boolean isRedirect) {
        AwContentsClientCallbackHelper.CancelCallbackPoller poller =
                mCallbackHelper.getCancelCallbackPoller();
        if (poller != null && poller.shouldCancelAllCallbacks()) return false;

        if (hasWebViewClient()) {
            // Note: only GET requests can be overridden, so we hardcode the method.
            AwWebResourceRequest request =
                    new AwWebResourceRequest(url, isMainFrame, hasUserGesture, "GET", null);
            request.isRedirect = isRedirect;
            return shouldOverrideUrlLoading(request);
        } else {
            return sendBrowsingIntent(context, url, hasUserGesture, isRedirect);
        }
    }

    private static boolean sendBrowsingIntent(Context context, String url, boolean hasUserGesture,
            boolean isRedirect) {
        if (!hasUserGesture && !isRedirect) {
            Log.w(TAG, "Denied starting an intent without a user gesture, URI %s", url);
            return true;
        }

        // Treat 'about:' URLs as internal, always open them in the WebView
        if (url.startsWith(ContentUrlConstants.ABOUT_URL_SHORT_PREFIX)) {
            return false;
        }

        Intent intent;
        // Perform generic parsing of the URI to turn it into an Intent.
        try {
            intent = Intent.parseUri(url, Intent.URI_INTENT_SCHEME);
        } catch (Exception ex) {
            Log.w(TAG, "Bad URI %s", url, ex);
            return false;
        }
        // Sanitize the Intent, ensuring web pages can not bypass browser
        // security (only access to BROWSABLE activities).
        intent.addCategory(Intent.CATEGORY_BROWSABLE);
        intent.setComponent(null);
        Intent selector = intent.getSelector();
        if (selector != null) {
            selector.addCategory(Intent.CATEGORY_BROWSABLE);
            selector.setComponent(null);
        }

        // Pass the package name as application ID so that the intent from the
        // same application can be opened in the same tab.
        intent.putExtra(Browser.EXTRA_APPLICATION_ID, context.getPackageName());

        // Check whether the context is activity context.
        if (ContextUtils.activityFromContext(context) == null) {
            Log.w(TAG, "Cannot call startActivity on non-activity context.");
            return false;
        }

        try {
            context.startActivity(intent);
            return true;
        } catch (ActivityNotFoundException ex) {
            Log.w(TAG, "No application can handle %s", url);
        } catch (SecurityException ex) {
            // This can happen if the Activity is exported="true", guarded by a permission, and sets
            // up an intent filter matching this intent. This is a valid configuration for an
            // Activity, so instead of crashing, we catch the exception and do nothing. See
            // https://crbug.com/808494 and https://crbug.com/889300.
            Log.w(TAG, "SecurityException when starting intent for %s", url);
        }

        return false;
    }

    public static Uri[] parseFileChooserResult(int resultCode, Intent intent) {
        if (resultCode == Activity.RESULT_CANCELED) {
            return null;
        }
        Uri result =
                intent == null || resultCode != Activity.RESULT_OK ? null : intent.getData();

        Uri[] uris = null;
        if (result != null) {
            uris = new Uri[1];
            uris[0] = result;
        }
        return uris;
    }

    /**
     * Type adaptation class for {@link android.webkit.FileChooserParams}.
     */
    public static class FileChooserParamsImpl {
        private int mMode;
        private String mAcceptTypes;
        private String mTitle;
        private String mDefaultFilename;
        private boolean mCapture;

        public FileChooserParamsImpl(int mode, String acceptTypes, String title,
                String defaultFilename, boolean capture) {
            mMode = mode;
            mAcceptTypes = acceptTypes;
            mTitle = title;
            mDefaultFilename = defaultFilename;
            mCapture = capture;
        }

        public String getAcceptTypesString() {
            return mAcceptTypes;
        }

        public int getMode() {
            return mMode;
        }

        public String[] getAcceptTypes() {
            if (mAcceptTypes == null) {
                return new String[0];
            }
            return mAcceptTypes.split(",");
        }

        public boolean isCaptureEnabled() {
            return mCapture;
        }

        public CharSequence getTitle() {
            return mTitle;
        }

        public String getFilenameHint() {
            return mDefaultFilename;
        }

        public Intent createIntent() {
            String mimeType = "*/*";
            if (mAcceptTypes != null && !mAcceptTypes.trim().isEmpty()) {
                mimeType = mAcceptTypes.split(",")[0];
            }

            Intent i = new Intent(Intent.ACTION_GET_CONTENT);
            i.addCategory(Intent.CATEGORY_OPENABLE);
            i.setType(mimeType);
            return i;
        }
    }

    public abstract void showFileChooser(
            Callback<String[]> uploadFilePathsCallback, FileChooserParamsImpl fileChooserParams);

    public abstract void onGeolocationPermissionsShowPrompt(
            String origin, AwGeolocationPermissions.Callback callback);

    public abstract void onGeolocationPermissionsHidePrompt();

    public abstract void onPermissionRequest(AwPermissionRequest awPermissionRequest);

    public abstract void onPermissionRequestCanceled(AwPermissionRequest awPermissionRequest);

    public abstract void onScaleChangedScaled(float oldScale, float newScale);

    protected abstract void handleJsAlert(String url, String message, JsResultReceiver receiver);

    protected abstract void handleJsBeforeUnload(String url, String message,
            JsResultReceiver receiver);

    protected abstract void handleJsConfirm(String url, String message, JsResultReceiver receiver);

    protected abstract void handleJsPrompt(String url, String message, String defaultValue,
            JsPromptResultReceiver receiver);

    protected abstract boolean onCreateWindow(boolean isDialog, boolean isUserGesture);

    protected abstract void onCloseWindow();

    public abstract void onReceivedTouchIconUrl(String url, boolean precomposed);

    public abstract void onReceivedIcon(Bitmap bitmap);

    public abstract void onReceivedTitle(String title);

    protected abstract void onRequestFocus();

    protected abstract View getVideoLoadingProgressView();

    public abstract void onPageStarted(String url);

    public abstract void onPageFinished(String url);

    public abstract void onPageCommitVisible(String url);

    public final void onReceivedError(AwWebResourceRequest request, AwWebResourceError error) {
        // Only one of these callbacks actually reaches out the client code. The first callback
        // is used on API versions up to and including L, the second on subsequent releases.
        // Below is the calls diagram:
        //
        //                           Old (<= L) glue              Old (<= L) android.webkit API
        //                             onReceivedError --------->   onReceivedError
        //  AwContentsClient           onReceivedError2 ->X   /
        //   abs. onReceivedError                            /
        //   abs. onReceivedError2                          /
        //                           New (M+) glue         /      New (M+) android.webkit API
        //                             onReceivedError    /     ->  onReceviedError <new>
        //   "->X" = "do nothing"        if (!<M API>) ---     /      if (isMainFrame) -\
        //                               else ->X             /       else ->X          |
        //                             onReceivedError2      /                          V
        //                               if (<M API>) -------       onReceivedError <old>
        //                               else ->X
        if (request.isMainFrame) {
            onReceivedError(error.errorCode, error.description, request.url);
        }
        onReceivedError2(request, error);

        // Record UMA on error code distribution here.
        RecordHistogram.recordSparseHistogram(
                "Android.WebView.onReceivedError.ErrorCode", error.errorCode);
    }

    protected abstract void onReceivedError(int errorCode, String description, String failingUrl);

    protected abstract void onReceivedError2(
            AwWebResourceRequest request, AwWebResourceError error);

    protected abstract void onSafeBrowsingHit(AwWebResourceRequest request, int threatType,
            Callback<AwSafeBrowsingResponse> callback);

    public abstract void onReceivedHttpError(AwWebResourceRequest request,
            AwWebResourceResponse response);

    public abstract void onShowCustomView(View view, CustomViewCallback callback);

    public abstract void onHideCustomView();

    public abstract Bitmap getDefaultVideoPoster();

    //--------------------------------------------------------------------------------------------
    //                              Other WebView-specific methods
    //--------------------------------------------------------------------------------------------
    //
    public abstract void onFindResultReceived(int activeMatchOrdinal, int numberOfMatches,
            boolean isDoneCounting);

    /**
     * Called whenever there is a new content picture available.
     * @param picture New picture.
     */
    public abstract void onNewPicture(Picture picture);

    public final void updateTitle(String title, boolean forceNotification) {
        if (!forceNotification && TextUtils.equals(mTitle, title)) return;
        mTitle = title;
        mCallbackHelper.postOnReceivedTitle(mTitle);
    }

    public abstract void onRendererUnresponsive(AwRenderProcess renderProcess);
    public abstract void onRendererResponsive(AwRenderProcess renderProcess);

    public abstract boolean onRenderProcessGone(AwRenderProcessGoneDetail detail);
}
