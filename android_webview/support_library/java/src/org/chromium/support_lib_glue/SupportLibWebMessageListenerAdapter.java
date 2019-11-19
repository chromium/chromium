// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_glue;

import android.net.Uri;
import android.webkit.WebView;

import org.chromium.android_webview.JsReplyProxy;
import org.chromium.android_webview.WebMessageListener;
import org.chromium.base.Log;
import org.chromium.content_public.browser.MessagePort;
import org.chromium.support_lib_boundary.WebMessageListenerBoundaryInterface;
import org.chromium.support_lib_boundary.util.BoundaryInterfaceReflectionUtil;
import org.chromium.support_lib_boundary.util.Features;

import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Proxy;

/**
 * Support library glue WebMessageListener.
 *
 * A new instance of this class is created transiently for every shared library WebViewCompat call.
 * Do not store state here.
 */
class SupportLibWebMessageListenerAdapter implements WebMessageListener {
    private static final String TAG = "WebMsgLtrAdptr";

    private final WebView mWebView;
    private WebMessageListenerBoundaryInterface mImpl;
    private String[] mSupportedFeatures;

    public SupportLibWebMessageListenerAdapter(
            WebView webView, /* WebMessageListener */ InvocationHandler handler) {
        mWebView = webView;
        mImpl = BoundaryInterfaceReflectionUtil.castToSuppLibClass(
                WebMessageListenerBoundaryInterface.class, handler);
        mSupportedFeatures = mImpl.getSupportedFeatures();
    }

    public /* WebMessageListener */ InvocationHandler getSupportLibInvocationHandler() {
        return Proxy.getInvocationHandler(mImpl);
    }

    @Override
    public void onPostMessage(final String message, final Uri sourceOrigin,
            final boolean isMainFrame, final JsReplyProxy replyProxy, final MessagePort[] ports) {
        if (!BoundaryInterfaceReflectionUtil.containsFeature(
                    mSupportedFeatures, Features.WEB_MESSAGE_LISTENER)) {
            Log.e(TAG, "The AndroidX doesn't have feature: " + Features.WEB_MESSAGE_LISTENER);
            return;
        }

        mImpl.onPostMessage(mWebView,
                BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                        new SupportLibWebMessageAdapter(message, ports)),
                sourceOrigin, isMainFrame,
                BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                        new SupportLibJsReplyProxyAdapter(replyProxy)));
    }
}
