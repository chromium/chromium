// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.content.ActivityNotFoundException;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.net.Uri;
import android.net.http.SslCertificate;
import android.net.http.SslError;

import org.jni_zero.CalledByNative;
import org.jni_zero.CalledByNativeUnchecked;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.android_webview.safe_browsing.AwSafeBrowsingConversionHelper;
import org.chromium.android_webview.safe_browsing.AwSafeBrowsingResponse;
import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.TraceEvent;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.components.embedder_support.util.WebResourceResponseInfo;
import org.chromium.net.NetError;

import java.security.Principal;
import java.security.PrivateKey;
import java.security.cert.CertificateEncodingException;
import java.security.cert.X509Certificate;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import javax.security.auth.x500.X500Principal;

/**
 * This class handles the JNI communication logic for the the AwContentsClient class.
 * Both the Java and the native peers of AwContentsClientBridge are owned by the
 * corresponding AwContents instances. This class and its native peer are connected
 * via weak references. The native AwContentsClientBridge sets up and clear these weak
 * references.
 */
@JNINamespace("android_webview")
public class AwContentsClientBridge {
    private static final String TAG = "AwContentsCB";

    private AwContentsClient mClient;
    private Context mContext;
    // The native peer of this object.
    private long mNativeContentsClientBridge;

    private final ClientCertLookupTable mLookupTable;

    // Used for mocking this class in tests.
    protected AwContentsClientBridge(ClientCertLookupTable table) {
        mLookupTable = table;
    }

    public AwContentsClientBridge(
            Context context, AwContentsClient client, ClientCertLookupTable table) {
        assert client != null;
        mContext = context;
        mClient = client;
        mLookupTable = table;
    }

    /**
     * Callback to communicate clientcertificaterequest back to the AwContentsClientBridge.
     * The public methods should be called on UI thread.
     * A request can not be proceeded, ignored  or canceled more than once. Doing this
     * is a programming error and causes an exception.
     */
    public class ClientCertificateRequestCallback {

        private final int mId;
        private final String mHost;
        private final int mPort;
        private boolean mIsCalled;

        public ClientCertificateRequestCallback(int id, String host, int port) {
            mId = id;
            mHost = host;
            mPort = port;
        }

        public void proceed(final PrivateKey privateKey, final X509Certificate[] chain) {
            PostTask.runOrPostTask(
                    TaskTraits.UI_DEFAULT, () -> proceedOnUiThread(privateKey, chain));
        }

        public void ignore() {
            PostTask.runOrPostTask(TaskTraits.UI_DEFAULT, () -> ignoreOnUiThread());
        }

        public void cancel() {
            PostTask.runOrPostTask(TaskTraits.UI_DEFAULT, () -> cancelOnUiThread());
        }

        private void proceedOnUiThread(PrivateKey privateKey, X509Certificate[] chain) {
            checkIfCalled();

            if (privateKey == null || chain == null || chain.length == 0) {
                Log.w(TAG, "Empty client certificate chain?");
                provideResponse(null, null);
                return;
            }
            // Encode the certificate chain.
            byte[][] encodedChain = new byte[chain.length][];
            try {
                for (int i = 0; i < chain.length; ++i) {
                    encodedChain[i] = chain[i].getEncoded();
                }
            } catch (CertificateEncodingException e) {
                Log.w(TAG, "Could not retrieve encoded certificate chain: " + e);
                provideResponse(null, null);
                return;
            }
            mLookupTable.allow(mHost, mPort, privateKey, encodedChain);
            provideResponse(privateKey, encodedChain);
        }

        private void ignoreOnUiThread() {
            checkIfCalled();
            provideResponse(null, null);
        }

        private void cancelOnUiThread() {
            checkIfCalled();
            mLookupTable.deny(mHost, mPort);
            provideResponse(null, null);
        }

        private void checkIfCalled() {
            if (mIsCalled) {
                throw new IllegalStateException("The callback was already called.");
            }
            mIsCalled = true;
        }

        private void provideResponse(PrivateKey privateKey, byte[][] certChain) {
            if (mNativeContentsClientBridge == 0) return;
            AwContentsClientBridgeJni.get()
                    .provideClientCertificateResponse(
                            mNativeContentsClientBridge,
                            AwContentsClientBridge.this,
                            mId,
                            certChain,
                            privateKey);
        }
    }

    // Used by the native peer to set/reset a weak ref to the native peer.
    @CalledByNative
    private void setNativeContentsClientBridge(long nativeContentsClientBridge) {
        mNativeContentsClientBridge = nativeContentsClientBridge;
    }

    // If returns false, the request is immediately canceled, and any call to proceedSslError
    // has no effect. If returns true, the request should be canceled or proceeded using
    // proceedSslError().
    // Unlike the webview classic, we do not keep keep a database of certificates that
    // are allowed by the user, because this functionality is already handled via
    // ssl_policy in native layers.
    @CalledByNative
    private boolean allowCertificateError(
            int certError, byte[] derBytes, final String url, final int id) {
        final SslCertificate cert = SslUtil.getCertificateFromDerBytes(derBytes);
        if (cert == null) {
            // if the certificate or the client is null, cancel the request
            return false;
        }
        final SslError sslError = SslUtil.sslErrorFromNetErrorCode(certError, cert, url);
        final Callback<Boolean> callback =
                value ->
                        PostTask.runOrPostTask(
                                TaskTraits.UI_DEFAULT,
                                () -> proceedSslError(value.booleanValue(), id));
        // Post the application callback back to the current thread to ensure the application
        // callback is executed without any native code on the stack. This so that any exception
        // thrown by the application callback won't have to be propagated through a native call
        // stack.
        AwThreadUtils.postToCurrentLooper(() -> mClient.onReceivedSslError(callback, sslError));
        return true;
    }

    private void proceedSslError(boolean proceed, int id) {
        if (mNativeContentsClientBridge == 0) return;
        AwContentsClientBridgeJni.get()
                .proceedSslError(
                        mNativeContentsClientBridge, AwContentsClientBridge.this, proceed, id);
    }

    // Intentionally not private for testing the native peer of this class.
    @CalledByNative
    protected void selectClientCertificate(
            final int id,
            final String[] keyTypes,
            byte[][] encodedPrincipals,
            final String host,
            final int port) {
        assert mNativeContentsClientBridge != 0;
        ClientCertLookupTable.Cert cert = mLookupTable.getCertData(host, port);
        if (mLookupTable.isDenied(host, port)) {
            AwContentsClientBridgeJni.get()
                    .provideClientCertificateResponse(
                            mNativeContentsClientBridge,
                            AwContentsClientBridge.this,
                            id,
                            null,
                            null);
            return;
        }
        if (cert != null) {
            AwContentsClientBridgeJni.get()
                    .provideClientCertificateResponse(
                            mNativeContentsClientBridge,
                            AwContentsClientBridge.this,
                            id,
                            cert.mCertChain,
                            cert.mPrivateKey);
            return;
        }
        // Build the list of principals from encoded versions.
        Principal[] principals = null;
        if (encodedPrincipals.length > 0) {
            principals = new X500Principal[encodedPrincipals.length];
            for (int n = 0; n < encodedPrincipals.length; n++) {
                try {
                    principals[n] = new X500Principal(encodedPrincipals[n]);
                } catch (IllegalArgumentException e) {
                    Log.w(TAG, "Exception while decoding issuers list: " + e);
                    AwContentsClientBridgeJni.get()
                            .provideClientCertificateResponse(
                                    mNativeContentsClientBridge,
                                    AwContentsClientBridge.this,
                                    id,
                                    null,
                                    null);
                    return;
                }
            }
        }

        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICallback.ON_RECEIVED_CLIENT_CERT_REQUEST")) {
            final ClientCertificateRequestCallback callback =
                    new ClientCertificateRequestCallback(id, host, port);
            mClient.onReceivedClientCertRequest(callback, keyTypes, principals, host, port);

            // Record UMA for onReceivedClientCertRequest.
            AwHistogramRecorder.recordCallbackInvocation(
                    AwHistogramRecorder.WebViewCallbackType.ON_RECEIVED_CLIENT_CERT_REQUEST);
        }
    }

    @CalledByNative
    private void handleJsAlert(final String url, final String message, final int id) {
        // Post the application callback back to the current thread to ensure the application
        // callback is executed without any native code on the stack. This so that any exception
        // thrown by the application callback won't have to be propagated through a native call
        // stack.
        AwThreadUtils.postToCurrentLooper(
                () -> {
                    JsResultHandler handler = new JsResultHandler(AwContentsClientBridge.this, id);
                    mClient.handleJsAlert(url, message, handler);
                });
    }

    @CalledByNative
    private void handleJsConfirm(final String url, final String message, final int id) {
        // Post the application callback back to the current thread to ensure the application
        // callback is executed without any native code on the stack. This so that any exception
        // thrown by the application callback won't have to be propagated through a native call
        // stack.
        AwThreadUtils.postToCurrentLooper(
                () -> {
                    JsResultHandler handler = new JsResultHandler(AwContentsClientBridge.this, id);
                    mClient.handleJsConfirm(url, message, handler);
                });
    }

    @CalledByNative
    private void handleJsPrompt(
            final String url, final String message, final String defaultValue, final int id) {
        // Post the application callback back to the current thread to ensure the application
        // callback is executed without any native code on the stack. This so that any exception
        // thrown by the application callback won't have to be propagated through a native call
        // stack.
        AwThreadUtils.postToCurrentLooper(
                () -> {
                    JsResultHandler handler = new JsResultHandler(AwContentsClientBridge.this, id);
                    mClient.handleJsPrompt(url, message, defaultValue, handler);
                });
    }

    @CalledByNative
    private void handleJsBeforeUnload(final String url, final String message, final int id) {
        // Post the application callback back to the current thread to ensure the application
        // callback is executed without any native code on the stack. This so that any exception
        // thrown by the application callback won't have to be propagated through a native call
        // stack.
        AwThreadUtils.postToCurrentLooper(
                () -> {
                    JsResultHandler handler = new JsResultHandler(AwContentsClientBridge.this, id);
                    mClient.handleJsBeforeUnload(url, message, handler);
                });
    }

    @CalledByNative
    private void newDownload(
            String url,
            String userAgent,
            String contentDisposition,
            String mimeType,
            long contentLength) {
        try (TraceEvent event = TraceEvent.scoped("WebView.APICallback.ON_DOWNLOAD_START")) {
            mClient.getCallbackHelper()
                    .postOnDownloadStart(
                            url, userAgent, contentDisposition, mimeType, contentLength);

            // Record UMA for onDownloadStart.
            AwHistogramRecorder.recordCallbackInvocation(
                    AwHistogramRecorder.WebViewCallbackType.ON_DOWNLOAD_START);
        }
    }

    @CalledByNative
    private void newLoginRequest(String realm, String account, String args) {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICallback.ON_RECEIVED_LOGIN_REQUEST")) {
            mClient.getCallbackHelper().postOnReceivedLoginRequest(realm, account, args);

            // Record UMA for onReceivedLoginRequest.
            AwHistogramRecorder.recordCallbackInvocation(
                    AwHistogramRecorder.WebViewCallbackType.ON_RECEIVED_LOGIN_REQUEST);
        }
    }

    @CalledByNative
    private void onReceivedError(
            // WebResourceRequest
            String url,
            boolean isOutermostMainFrame,
            boolean hasUserGesture,
            boolean isRendererInitiated,
            String method,
            String[] requestHeaderNames,
            String[] requestHeaderValues,
            // WebResourceError
            @NetError int errorCode,
            String description,
            boolean safebrowsingHit,
            boolean shouldOmitNotificationsForSafeBrowsingHit) {
        AwContentsClient.AwWebResourceRequest request =
                new AwContentsClient.AwWebResourceRequest(
                        url,
                        isOutermostMainFrame,
                        hasUserGesture,
                        method,
                        requestHeaderNames,
                        requestHeaderValues);
        AwContentsClient.AwWebResourceError error = new AwContentsClient.AwWebResourceError();
        error.errorCode = ErrorCodeConversionHelper.convertErrorCode(errorCode);
        error.description = description;

        String unreachableWebDataUrl = AwContentsStatics.getUnreachableWebDataUrl();
        boolean isErrorUrl =
                unreachableWebDataUrl != null && unreachableWebDataUrl.equals(request.url);

        if ((!isErrorUrl && errorCode != NetError.ERR_ABORTED) || safebrowsingHit) {
            // NetError.ERR_ABORTED error code is generated for the following reasons:
            // - WebView.stopLoading is called;
            // - the navigation is intercepted by the embedder via shouldOverrideUrlLoading;
            // - server returned 204 status (no content).
            //
            // Android WebView does not notify the embedder of these situations using
            // this error code with the WebViewClient.onReceivedError callback.
            if (safebrowsingHit) {
                if (shouldOmitNotificationsForSafeBrowsingHit) {
                    // With committed interstitials we don't fire these notifications when the
                    // interstitial shows, we instead handle them once the interstitial is
                    // dismissed.
                    return;
                } else {
                    error.errorCode = WebviewErrorCode.ERROR_UNSAFE_RESOURCE;
                }
            }
            if (request.isOutermostMainFrame
                    && AwComputedFlags.pageStartedOnCommitEnabled(isRendererInitiated)) {
                mClient.getCallbackHelper().postOnPageStarted(request.url);
            }
            mClient.getCallbackHelper().postOnReceivedError(request, error);
            if (request.isOutermostMainFrame) {
                // Need to call onPageFinished after onReceivedError for backwards compatibility
                // with the classic webview. See also AwWebContentsObserver.didFailLoad which is
                // used when we want to send onPageFinished alone.
                mClient.getCallbackHelper().postOnPageFinished(request.url);
            }
        }
    }

    @CalledByNative
    public void onSafeBrowsingHit(
            // WebResourceRequest
            String url,
            boolean isOutermostMainFrame,
            boolean hasUserGesture,
            String method,
            String[] requestHeaderNames,
            String[] requestHeaderValues,
            int threatType,
            final int requestId) {
        AwContentsClient.AwWebResourceRequest request =
                new AwContentsClient.AwWebResourceRequest(
                        url,
                        isOutermostMainFrame,
                        hasUserGesture,
                        method,
                        requestHeaderNames,
                        requestHeaderValues);

        Callback<AwSafeBrowsingResponse> callback =
                response ->
                        PostTask.runOrPostTask(
                                TaskTraits.UI_DEFAULT,
                                () ->
                                        AwContentsClientBridgeJni.get()
                                                .takeSafeBrowsingAction(
                                                        mNativeContentsClientBridge,
                                                        AwContentsClientBridge.this,
                                                        response.action(),
                                                        response.reporting(),
                                                        requestId));

        int webViewThreatType = AwSafeBrowsingConversionHelper.convertThreatType(threatType);
        mClient.getCallbackHelper().postOnSafeBrowsingHit(request, webViewThreatType, callback);
    }

    @CalledByNative
    private void onReceivedHttpError(
            // WebResourceRequest
            String url,
            boolean isOutermostMainFrame,
            boolean hasUserGesture,
            String method,
            String[] requestHeaderNames,
            String[] requestHeaderValues,
            // WebResourceResponse
            String mimeType,
            String encoding,
            int statusCode,
            String reasonPhrase,
            String[] responseHeaderNames,
            String[] responseHeaderValues) {
        AwContentsClient.AwWebResourceRequest request =
                new AwContentsClient.AwWebResourceRequest(
                        url,
                        isOutermostMainFrame,
                        hasUserGesture,
                        method,
                        requestHeaderNames,
                        requestHeaderValues);
        Map<String, String> responseHeaders =
                new HashMap<String, String>(responseHeaderNames.length);
        // Note that we receive un-coalesced response header lines, thus we need to combine
        // values for the same header.
        for (int i = 0; i < responseHeaderNames.length; ++i) {
            if (!responseHeaders.containsKey(responseHeaderNames[i])) {
                responseHeaders.put(responseHeaderNames[i], responseHeaderValues[i]);
            } else if (!responseHeaderValues[i].isEmpty()) {
                String currentValue = responseHeaders.get(responseHeaderNames[i]);
                if (!currentValue.isEmpty()) {
                    currentValue += ", ";
                }
                responseHeaders.put(responseHeaderNames[i], currentValue + responseHeaderValues[i]);
            }
        }
        WebResourceResponseInfo response =
                new WebResourceResponseInfo(
                        mimeType, encoding, null, statusCode, reasonPhrase, responseHeaders);
        mClient.getCallbackHelper().postOnReceivedHttpError(request, response);
    }

    @CalledByNativeUnchecked
    private boolean shouldOverrideUrlLoading(
            String url,
            boolean hasUserGesture,
            boolean isRedirect,
            String[] requestHeaderNames,
            String[] requestHeaderValues,
            boolean isOutermostMainFrame) {
        HashMap<String, String> requestHeaders = null;
        if (requestHeaderNames.length > 0) {
            requestHeaders = new HashMap<String, String>(requestHeaderNames.length);
            for (int i = 0; i < requestHeaderNames.length; ++i) {
                assert !requestHeaders.containsKey(requestHeaderNames[i]);
                assert !requestHeaderValues[i].isEmpty();
                requestHeaders.put(requestHeaderNames[i], requestHeaderValues[i]);
            }
        }
        return mClient.shouldIgnoreNavigation(
                mContext, url, isOutermostMainFrame, hasUserGesture, requestHeaders, isRedirect);
    }

    @CalledByNative
    private boolean sendBrowseIntent(String url) {
        try {
            Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(url));
            intent.addCategory(Intent.CATEGORY_BROWSABLE);
            intent.addCategory(Intent.CATEGORY_DEFAULT);
            if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.R) {
                intent.setFlags(
                        Intent.FLAG_ACTIVITY_REQUIRE_NON_BROWSER
                                | Intent.FLAG_ACTIVITY_REQUIRE_DEFAULT);
            } else {
                ResolveInfo bestActivity = getBestActivityForIntent(intent);
                if (bestActivity == null) {
                    return false;
                }
                intent.setComponent(
                        new ComponentName(
                                bestActivity.activityInfo.packageName,
                                bestActivity.activityInfo.name));
            }
            mContext.startActivity(intent);
            return true;
        } catch (ActivityNotFoundException e) {
            Log.w(TAG, "Could not find an application to handle : %s", url);
        } catch (Exception e) {
            Log.e(TAG, "Exception while sending browse Intent.", e);
        }
        return false;
    }

    private ResolveInfo getBestActivityForIntent(Intent intent) {
        List<ResolveInfo> resolveInfos =
                mContext.getPackageManager()
                        .queryIntentActivities(intent, PackageManager.GET_RESOLVED_FILTER);

        ResolveInfo bestActivity = null;
        final int n = resolveInfos.size();

        if (n == 1) {
            bestActivity = resolveInfos.get(0);
        } else if (n > 1) {
            ResolveInfo r0 = resolveInfos.get(0);
            ResolveInfo r1 = resolveInfos.get(1);

            // If the first activity has a higher priority, or a different
            // default, then it is always desirable to pick it, else there is a tie
            // between the first and second activity and we cant choose one(best one).
            if (r0.priority > r1.priority
                    || r0.preferredOrder > r1.preferredOrder
                    || r0.isDefault != r1.isDefault) {
                bestActivity = resolveInfos.get(0);
            }
        }
        // Different cases due to which we return null from here
        // 1. There is no activity to handle this intent
        // 2. We can't come down to a single higher priority activity
        // 3. Best activity to handle is actually a browser.
        if (bestActivity == null || isBrowserApp(bestActivity)) {
            return null;
        }
        return bestActivity;
    }

    private boolean isBrowserApp(ResolveInfo ri) {
        if (ri.filter.hasCategory(Intent.CATEGORY_APP_BROWSER)
                || (ri.filter.hasDataScheme("http")
                        && ri.filter.hasDataScheme("https")
                        && ri.filter.countDataAuthorities() == 0)) {
            return true;
        }
        return false;
    }

    void confirmJsResult(int id, String prompt) {
        if (mNativeContentsClientBridge == 0) return;
        AwContentsClientBridgeJni.get()
                .confirmJsResult(
                        mNativeContentsClientBridge, AwContentsClientBridge.this, id, prompt);
    }

    void cancelJsResult(int id) {
        if (mNativeContentsClientBridge == 0) return;
        AwContentsClientBridgeJni.get()
                .cancelJsResult(mNativeContentsClientBridge, AwContentsClientBridge.this, id);
    }

    @NativeMethods
    interface Natives {
        void takeSafeBrowsingAction(
                long nativeAwContentsClientBridge,
                AwContentsClientBridge caller,
                int action,
                boolean reporting,
                int requestId);

        void proceedSslError(
                long nativeAwContentsClientBridge,
                AwContentsClientBridge caller,
                boolean proceed,
                int id);

        void provideClientCertificateResponse(
                long nativeAwContentsClientBridge,
                AwContentsClientBridge caller,
                int id,
                byte[][] certChain,
                PrivateKey androidKey);

        void confirmJsResult(
                long nativeAwContentsClientBridge,
                AwContentsClientBridge caller,
                int id,
                String prompt);

        void cancelJsResult(
                long nativeAwContentsClientBridge, AwContentsClientBridge caller, int id);
    }
}
