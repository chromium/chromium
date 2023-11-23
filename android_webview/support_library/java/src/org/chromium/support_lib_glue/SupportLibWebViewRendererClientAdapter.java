// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.support_lib_glue;

import android.webkit.WebView;

import com.android.webview.chromium.SharedWebViewRendererClientAdapter;

import org.chromium.android_webview.AwRenderProcess;
import org.chromium.android_webview.common.Lifetime;
import org.chromium.support_lib_boundary.WebViewRendererClientBoundaryInterface;
import org.chromium.support_lib_boundary.util.BoundaryInterfaceReflectionUtil;
import org.chromium.support_lib_boundary.util.Features;

import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Proxy;

/**
 * Support library glue renderer client callback adapter.
 *
 * A new instance of this class is created transiently for every shared library
 * WebViewCompat call. Do not store state here.
 */
@Lifetime.WebView
class SupportLibWebViewRendererClientAdapter extends SharedWebViewRendererClientAdapter {
    private WebViewRendererClientBoundaryInterface mImpl;
    private String[] mSupportedFeatures;

    public SupportLibWebViewRendererClientAdapter(
            /* WebViewRendererClient */ InvocationHandler invocationHandler) {
        mImpl =
                BoundaryInterfaceReflectionUtil.castToSuppLibClass(
                        WebViewRendererClientBoundaryInterface.class, invocationHandler);
        mSupportedFeatures = mImpl.getSupportedFeatures();
    }

    @Override
    public /* WebViewRendererClient */ InvocationHandler getSupportLibInvocationHandler() {
        return Proxy.getInvocationHandler(mImpl);
    }

    @Override
    public void onRendererUnresponsive(final WebView webView, final AwRenderProcess renderProcess) {
        if (!BoundaryInterfaceReflectionUtil.containsFeature(
                mSupportedFeatures, Features.WEB_VIEW_RENDERER_CLIENT_BASIC_USAGE)) {
            return;
        }
        mImpl.onRendererUnresponsive(
                webView,
                BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                        new SupportLibWebViewRendererAdapter(renderProcess)));
    }

    @Override
    public void onRendererResponsive(final WebView webView, final AwRenderProcess renderProcess) {
        if (!BoundaryInterfaceReflectionUtil.containsFeature(
                mSupportedFeatures, Features.WEB_VIEW_RENDERER_CLIENT_BASIC_USAGE)) {
            return;
        }
        mImpl.onRendererResponsive(
                webView,
                BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                        new SupportLibWebViewRendererAdapter(renderProcess)));
    }
}
