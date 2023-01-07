// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_glue;

import static org.chromium.support_lib_glue.SupportLibWebViewChromiumFactory.recordApiCall;

import android.net.Uri;
import android.webkit.WebChromeClient;
import android.webkit.WebView;
import android.webkit.WebViewClient;

import com.android.webview.chromium.SharedWebViewChromium;
import com.android.webview.chromium.SharedWebViewRendererClientAdapter;
import com.android.webview.chromium.WebkitToSharedGlueConverter;

import org.chromium.android_webview.AwContents;
import org.chromium.support_lib_boundary.VisualStateCallbackBoundaryInterface;
import org.chromium.support_lib_boundary.WebMessageBoundaryInterface;
import org.chromium.support_lib_boundary.WebViewProviderBoundaryInterface;
import org.chromium.support_lib_boundary.util.BoundaryInterfaceReflectionUtil;
import org.chromium.support_lib_glue.SupportLibWebViewChromiumFactory.ApiCall;

import java.lang.reflect.InvocationHandler;

/**
 * Support library glue version of WebViewChromium.
 *
 * A new instance of this class is created transiently for every shared library
 * WebViewCompat call. Do not store state here.
 */
class SupportLibWebViewChromium implements WebViewProviderBoundaryInterface {
    private final WebView mWebView;
    private final SharedWebViewChromium mSharedWebViewChromium;

    public SupportLibWebViewChromium(WebView webView) {
        mWebView = webView;
        mSharedWebViewChromium = WebkitToSharedGlueConverter.getSharedWebViewChromium(webView);
    }

    @Override
    public void insertVisualStateCallback(long requestId, InvocationHandler callbackInvoHandler) {
        recordApiCall(ApiCall.INSERT_VISUAL_STATE_CALLBACK);
        final VisualStateCallbackBoundaryInterface visualStateCallback =
                BoundaryInterfaceReflectionUtil.castToSuppLibClass(
                        VisualStateCallbackBoundaryInterface.class, callbackInvoHandler);

        mSharedWebViewChromium.insertVisualStateCallback(
                requestId, new AwContents.VisualStateCallback() {
                    @Override
                    public void onComplete(long requestId) {
                        visualStateCallback.onComplete(requestId);
                    }
                });
    }

    @Override
    public /* WebMessagePort */ InvocationHandler[] createWebMessageChannel() {
        return SupportLibWebMessagePortAdapter.fromMessagePorts(
                mSharedWebViewChromium.createWebMessageChannel());
    }

    @Override
    public void postMessageToMainFrame(
            /* WebMessage */ InvocationHandler message, Uri targetOrigin) {
        recordApiCall(ApiCall.POST_MESSAGE_TO_MAIN_FRAME);
        WebMessageBoundaryInterface messageBoundaryInterface =
                BoundaryInterfaceReflectionUtil.castToSuppLibClass(
                        WebMessageBoundaryInterface.class, message);
        mSharedWebViewChromium.postMessageToMainFrame(
                SupportLibWebMessagePayloadAdapter.fromWebMessageBoundaryInterface(
                        messageBoundaryInterface),
                targetOrigin.toString(),
                SupportLibWebMessagePortAdapter.toMessagePorts(
                        messageBoundaryInterface.getPorts()));
    }

    @Override
    public void addWebMessageListener(String jsObjectName, String[] allowedOriginRules,
            /* WebMessageListener */ InvocationHandler listener) {
        recordApiCall(ApiCall.ADD_WEB_MESSAGE_LISTENER);
        mSharedWebViewChromium.addWebMessageListener(jsObjectName, allowedOriginRules,
                new SupportLibWebMessageListenerAdapter(mWebView, listener));
    }

    @Override
    public void removeWebMessageListener(final String jsObjectName) {
        recordApiCall(ApiCall.REMOVE_WEB_MESSAGE_LISTENER);
        mSharedWebViewChromium.removeWebMessageListener(jsObjectName);
    }

    @Override
    public /* ScriptHandler */ InvocationHandler addDocumentStartJavaScript(
            final String script, final String[] allowedOriginRules) {
        recordApiCall(ApiCall.ADD_DOCUMENT_START_SCRIPT);
        return BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                new SupportLibScriptHandlerAdapter(
                        mSharedWebViewChromium.addDocumentStartJavaScript(
                                script, allowedOriginRules)));
    }

    @Override
    public WebViewClient getWebViewClient() {
        recordApiCall(ApiCall.GET_WEBVIEW_CLIENT);
        return mSharedWebViewChromium.getWebViewClient();
    }

    @Override
    public WebChromeClient getWebChromeClient() {
        recordApiCall(ApiCall.GET_WEBCHROME_CLIENT);
        return mSharedWebViewChromium.getWebChromeClient();
    }

    @Override
    public /* WebViewRenderer */ InvocationHandler getWebViewRenderer() {
        recordApiCall(ApiCall.GET_WEBVIEW_RENDERER);
        return BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                new SupportLibWebViewRendererAdapter(mSharedWebViewChromium.getRenderProcess()));
    }

    @Override
    public /* WebViewRendererClient */ InvocationHandler getWebViewRendererClient() {
        recordApiCall(ApiCall.GET_WEBVIEW_RENDERER_CLIENT);
        SharedWebViewRendererClientAdapter webViewRendererClientAdapter =
                mSharedWebViewChromium.getWebViewRendererClientAdapter();
        return webViewRendererClientAdapter != null
                ? webViewRendererClientAdapter.getSupportLibInvocationHandler()
                : null;
    }

    @Override
    public void setWebViewRendererClient(
            /* WebViewRendererClient */ InvocationHandler webViewRendererClient) {
        recordApiCall(ApiCall.SET_WEBVIEW_RENDERER_CLIENT);
        mSharedWebViewChromium.setWebViewRendererClientAdapter(webViewRendererClient != null
                        ? new SupportLibWebViewRendererClientAdapter(webViewRendererClient)
                        : null);
    }
}
