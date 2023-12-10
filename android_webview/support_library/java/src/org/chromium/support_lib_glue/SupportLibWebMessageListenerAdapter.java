// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_glue;

import android.net.Uri;
import android.webkit.WebView;

import org.chromium.android_webview.JsReplyProxy;
import org.chromium.android_webview.WebMessageListener;
import org.chromium.android_webview.common.Lifetime;
import org.chromium.base.Log;
import org.chromium.content_public.browser.MessagePayload;
import org.chromium.content_public.browser.MessagePayloadType;
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
@Lifetime.Temporary
class SupportLibWebMessageListenerAdapter implements WebMessageListener {
    private static final String TAG = "WebMsgLtrAdptr";

    private final WebView mWebView;
    private final WebMessageListenerBoundaryInterface mImpl;
    private final String[] mSupportedFeatures;

    public SupportLibWebMessageListenerAdapter(
            WebView webView, /* WebMessageListener */ InvocationHandler handler) {
        mWebView = webView;
        mImpl =
                BoundaryInterfaceReflectionUtil.castToSuppLibClass(
                        WebMessageListenerBoundaryInterface.class, handler);
        mSupportedFeatures = mImpl.getSupportedFeatures();
    }

    public /* WebMessageListener */ InvocationHandler getSupportLibInvocationHandler() {
        return Proxy.getInvocationHandler(mImpl);
    }

    @Override
    public void onPostMessage(
            final MessagePayload payload,
            final Uri topLevelOrigin,
            final Uri sourceOrigin,
            final boolean isMainFrame,
            final JsReplyProxy replyProxy,
            final MessagePort[] ports) {
        if (!BoundaryInterfaceReflectionUtil.containsFeature(
                mSupportedFeatures, Features.WEB_MESSAGE_LISTENER)) {
            Log.e(TAG, "The AndroidX doesn't have feature: " + Features.WEB_MESSAGE_LISTENER);
            return;
        }

        if (payload.getType() == MessagePayloadType.STRING
                || (payload.getType() == MessagePayloadType.ARRAY_BUFFER
                        && BoundaryInterfaceReflectionUtil.containsFeature(
                                mSupportedFeatures, Features.WEB_MESSAGE_ARRAY_BUFFER))) {
            mImpl.onPostMessage(
                    mWebView,
                    BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                            new SupportLibWebMessageAdapter(payload, ports)),
                    sourceOrigin,
                    isMainFrame,
                    BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                            new SupportLibJsReplyProxyAdapter(replyProxy)));
        } else {
            Log.e(
                    TAG,
                    "The AndroidX doesn't support payload type: "
                            + MessagePayload.typeToString(payload.getType()));
        }
    }
}
