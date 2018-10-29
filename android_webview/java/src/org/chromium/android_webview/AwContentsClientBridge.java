// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.content.Context;
import android.net.http.SslCertificate;
import android.net.http.SslError;
import android.os.Handler;
import android.util.Log;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.CalledByNativeUnchecked;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.net.NetError;

import java.security.Principal;
import java.security.PrivateKey;
import java.security.cert.CertificateEncodingException;
import java.security.cert.X509Certificate;
import java.util.HashMap;
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
    private static final String TAG = "AwContentsClientBridge";

    private AwContentsClient mClient;
    private Context mContext;
    // The native peer of this object.
    private long mNativeContentsClientBridge;

    private final ClientCertLookupTable mLookupTable;

    // Used for mocking this class in tests.
    protected AwContentsClientBridge(ClientCertLookupTable table) {
        mLookupTable = table;
    }

    public AwContentsClientBridge(Context context, AwContentsClient client,
            ClientCertLookupTable table) {
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
            ThreadUtils.runOnUiThread(() -> proceedOnUiThread(privateKey, chain));
        }

        public void ignore() {
            ThreadUtils.runOnUiThread(() -> ignoreOnUiThread());
        }

        public void cancel() {
            ThreadUtils.runOnUiThread(() -> cancelOnUiThread());
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
            nativeProvideClientCertificateResponse(mNativeContentsClientBridge, mId,
                    certChain, privateKey);
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
    private boolean allowCertificateError(int certError, byte[] derBytes, final String url,
            final int id) {
        final SslCertificate cert = SslUtil.getCertificateFromDerBytes(derBytes);
        if (cert == null) {
            // if the certificate or the client is null, cancel the request
            return false;
        }
        final SslError sslError = SslUtil.sslErrorFromNetErrorCode(certError, cert, url);
        final Callback<Boolean> callback =
                value -> ThreadUtils.runOnUiThread(() -> proceedSslError(value.booleanValue(), id));
        // Post the application callback back to the current thread to ensure the application
        // callback is executed without any native code on the stack. This so that any exception
        // thrown by the application callback won't have to be propagated through a native call
        // stack.
        new Handler().post(() -> mClient.onReceivedSslError(callback, sslError));
        return true;
    }

    private void proceedSslError(boolean proceed, int id) {
        if (mNativeContentsClientBridge == 0) return;
        nativeProceedSslError(mNativeContentsClientBridge, proceed, id);
    }

    // Intentionally not private for testing the native peer of this class.
    @CalledByNative
    protected void selectClientCertificate(final int id, final String[] keyTypes,
            byte[][] encodedPrincipals, final String host, final int port) {
        assert mNativeContentsClientBridge != 0;
        ClientCertLookupTable.Cert cert = mLookupTable.getCertData(host, port);
        if (mLookupTable.isDenied(host, port)) {
            nativeProvideClientCertificateResponse(mNativeContentsClientBridge, id,
                    null, null);
            return;
        }
        if (cert != null) {
            nativeProvideClientCertificateResponse(mNativeContentsClientBridge, id,
                    cert.mCertChain, cert.mPrivateKey);
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
                    nativeProvideClientCertificateResponse(mNativeContentsClientBridge, id,
                            null, null);
                    return;
                }
            }

        }

        final ClientCertificateRequestCallback callback =
                new ClientCertificateRequestCallback(id, host, port);
        mClient.onReceivedClientCertRequest(callback, keyTypes, principals, host, port);
    }

    @CalledByNative
    private void handleJsAlert(final String url, final String message, final int id) {
        // Post the application callback back to the current thread to ensure the application
        // callback is executed without any native code on the stack. This so that any exception
        // thrown by the application callback won't have to be propagated through a native call
        // stack.
        new Handler().post(() -> {
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
        new Handler().post(() -> {
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
        new Handler().post(() -> {
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
        new Handler().post(() -> {
            JsResultHandler handler = new JsResultHandler(AwContentsClientBridge.this, id);
            mClient.handleJsBeforeUnload(url, message, handler);
        });
    }

    @CalledByNative
    private void newDownload(String url, String userAgent, String contentDisposition,
            String mimeType, long contentLength) {
        mClient.getCallbackHelper().postOnDownloadStart(
                url, userAgent, contentDisposition, mimeType, contentLength);
    }

    @CalledByNative
    private void newLoginRequest(String realm, String account, String args) {
        mClient.getCallbackHelper().postOnReceivedLoginRequest(realm, account, args);
    }

    @CalledByNative
    private void onReceivedError(
            // WebResourceRequest
            String url, boolean isMainFrame, boolean hasUserGesture, boolean isRendererInitiated,
            String method, String[] requestHeaderNames, String[] requestHeaderValues,
            // WebResourceError
            int errorCode, String description, boolean safebrowsingHit) {
        AwContentsClient.AwWebResourceRequest request = new AwContentsClient.AwWebResourceRequest(
                url, isMainFrame, hasUserGesture, method, requestHeaderNames, requestHeaderValues);
        AwContentsClient.AwWebResourceError error = new AwContentsClient.AwWebResourceError();
        error.errorCode = errorCode;
        error.description = description;

        String unreachableWebDataUrl = AwContentsStatics.getUnreachableWebDataUrl();
        boolean isErrorUrl =
                unreachableWebDataUrl != null && unreachableWebDataUrl.equals(request.url);

        if ((!isErrorUrl && error.errorCode != NetError.ERR_ABORTED) || safebrowsingHit) {
            // NetError.ERR_ABORTED error code is generated for the following reasons:
            // - WebView.stopLoading is called;
            // - the navigation is intercepted by the embedder via shouldOverrideUrlLoading;
            // - server returned 204 status (no content).
            //
            // Android WebView does not notify the embedder of these situations using
            // this error code with the WebViewClient.onReceivedError callback.
            if (safebrowsingHit) {
                error.errorCode = ErrorCodeConversionHelper.ERROR_UNSAFE_RESOURCE;
            } else {
                error.errorCode = ErrorCodeConversionHelper.convertErrorCode(error.errorCode);
            }
            if (request.isMainFrame && isRendererInitiated) {
                mClient.getCallbackHelper().postOnPageStarted(request.url);
            }
            mClient.getCallbackHelper().postOnReceivedError(request, error);
            if (request.isMainFrame) {
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
            String url, boolean isMainFrame, boolean hasUserGesture, String method,
            String[] requestHeaderNames, String[] requestHeaderValues, int threatType,
            final int requestId) {
        AwContentsClient.AwWebResourceRequest request = new AwContentsClient.AwWebResourceRequest(
                url, isMainFrame, hasUserGesture, method, requestHeaderNames, requestHeaderValues);

        // TODO(ntfschr): remove clang-format directives once crbug/764582 is resolved
        // clang-format off
        Callback<AwSafeBrowsingResponse> callback =
                response -> ThreadUtils.runOnUiThread(
                        () -> nativeTakeSafeBrowsingAction(mNativeContentsClientBridge,
                                response.action(), response.reporting(), requestId));
        // clang-format on

        mClient.getCallbackHelper().postOnSafeBrowsingHit(
                request, AwSafeBrowsingConversionHelper.convertThreatType(threatType), callback);
    }

    @CalledByNative
    private void onReceivedHttpError(
            // WebResourceRequest
            String url, boolean isMainFrame, boolean hasUserGesture, String method,
            String[] requestHeaderNames, String[] requestHeaderValues,
            // WebResourceResponse
            String mimeType, String encoding, int statusCode, String reasonPhrase,
            String[] responseHeaderNames, String[] responseHeaderValues) {
        AwContentsClient.AwWebResourceRequest request = new AwContentsClient.AwWebResourceRequest(
                url, isMainFrame, hasUserGesture, method, requestHeaderNames, requestHeaderValues);
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
        AwWebResourceResponse response = new AwWebResourceResponse(
                mimeType, encoding, null, statusCode, reasonPhrase, responseHeaders);
        mClient.getCallbackHelper().postOnReceivedHttpError(request, response);
    }

    @CalledByNativeUnchecked
    private boolean shouldOverrideUrlLoading(
            String url, boolean hasUserGesture, boolean isRedirect, boolean isMainFrame) {
        return mClient.shouldIgnoreNavigation(
                mContext, url, isMainFrame, hasUserGesture, isRedirect);
    }

    void confirmJsResult(int id, String prompt) {
        if (mNativeContentsClientBridge == 0) return;
        nativeConfirmJsResult(mNativeContentsClientBridge, id, prompt);
    }

    void cancelJsResult(int id) {
        if (mNativeContentsClientBridge == 0) return;
        nativeCancelJsResult(mNativeContentsClientBridge, id);
    }

    //--------------------------------------------------------------------------------------------
    //  Native methods
    //--------------------------------------------------------------------------------------------
    private native void nativeTakeSafeBrowsingAction(
            long nativeAwContentsClientBridge, int action, boolean reporting, int requestId);
    private native void nativeProceedSslError(long nativeAwContentsClientBridge, boolean proceed,
            int id);
    private native void nativeProvideClientCertificateResponse(long nativeAwContentsClientBridge,
            int id, byte[][] certChain, PrivateKey androidKey);

    private native void nativeConfirmJsResult(long nativeAwContentsClientBridge, int id,
            String prompt);
    private native void nativeCancelJsResult(long nativeAwContentsClientBridge, int id);
}
