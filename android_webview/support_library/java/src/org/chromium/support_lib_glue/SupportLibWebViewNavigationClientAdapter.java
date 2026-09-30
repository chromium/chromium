// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.support_lib_glue;

import org.chromium.android_webview.AwNavigationListener;
import org.chromium.android_webview.AwNavigationState;
import org.chromium.android_webview.AwPageState;
import org.chromium.android_webview.common.Lifetime;
import org.chromium.build.annotations.NullMarked;
import org.chromium.support_lib_boundary.WebViewNavigationClientBoundaryInterface;
import org.chromium.support_lib_boundary.util.BoundaryInterfaceReflectionUtil;
import org.chromium.support_lib_boundary.util.Features;

import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Proxy;
import java.util.Map;

/**
 * Support library glue navigation client callback adapter.
 *
 * <p>A new instance of this class is created transiently for every shared library WebViewCompat
 * call. Do not store state here.
 */
@Lifetime.Temporary
@NullMarked
class SupportLibWebViewNavigationClientAdapter implements AwNavigationListener {
    private final WebViewNavigationClientBoundaryInterface mClientImpl;
    private final String[] mSupportedFeatures;

    public SupportLibWebViewNavigationClientAdapter(
            /* WebViewNavigationClient */ InvocationHandler invocationHandler) {
        mClientImpl =
                BoundaryInterfaceReflectionUtil.castToSuppLibClass(
                        WebViewNavigationClientBoundaryInterface.class, invocationHandler);
        mSupportedFeatures = mClientImpl.getSupportedFeatures();
    }

    @Override
    public /* WebViewNavigationClient */ InvocationHandler getSupportLibInvocationHandler() {
        return Proxy.getInvocationHandler(mClientImpl);
    }

    @Override
    public void onNavigationStarted(AwNavigationState navigationState) {
        if (!BoundaryInterfaceReflectionUtil.containsFeature(
                mSupportedFeatures, Features.WEB_VIEW_NAVIGATION_CLIENT_BASIC_USAGE)) {
            return;
        }
        mClientImpl.onNavigationStarted(
                BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                        new SupportLibWebViewNavigationAdapter(navigationState.getNavigation())));
    }

    @Override
    public void onNavigationRedirected(
            AwNavigationState navigationState,
            Map<String, String> responseHeaders,
            int statusCode) {
        if (!BoundaryInterfaceReflectionUtil.containsFeature(
                mSupportedFeatures, Features.WEB_VIEW_NAVIGATION_CLIENT_BASIC_USAGE)) {
            return;
        }
        // The redirect response headers and status code are not exposed here, as this navigation
        // client is set to be deprecated in favour of {@link #AwNavigationListener}.
        mClientImpl.onNavigationRedirected(
                BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                        new SupportLibWebViewNavigationAdapter(navigationState.getNavigation())));
    }

    @Override
    public void onNavigationCompleted(AwNavigationState navigationState) {
        if (!BoundaryInterfaceReflectionUtil.containsFeature(
                mSupportedFeatures, Features.WEB_VIEW_NAVIGATION_CLIENT_BASIC_USAGE)) {
            return;
        }
        mClientImpl.onNavigationCompleted(
                BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                        new SupportLibWebViewNavigationAdapter(navigationState.getNavigation())));
    }

    // Not implemented as this navigation client is set to be deprecated in favour of
    // {@link #AwNavigationListener}
    @Override
    public void onNavigationVisible(AwNavigationState navigationState) {}

    @Override
    public void onPageDeleted(AwPageState pageState) {
        if (!BoundaryInterfaceReflectionUtil.containsFeature(
                mSupportedFeatures, Features.WEB_VIEW_NAVIGATION_CLIENT_BASIC_USAGE)) {
            return;
        }
        mClientImpl.onPageDeleted(
                BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                        new SupportLibWebViewPageAdapter(pageState.getPage())));
    }

    @Override
    public void onPageLoadEventFired(AwPageState pageState) {
        if (!BoundaryInterfaceReflectionUtil.containsFeature(
                mSupportedFeatures, Features.WEB_VIEW_NAVIGATION_CLIENT_BASIC_USAGE)) {
            return;
        }
        mClientImpl.onPageLoadEventFired(
                BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                        new SupportLibWebViewPageAdapter(pageState.getPage())));
    }

    @Override
    public void onPageDOMContentLoadedEventFired(AwPageState pageState) {
        if (!BoundaryInterfaceReflectionUtil.containsFeature(
                mSupportedFeatures, Features.WEB_VIEW_NAVIGATION_CLIENT_BASIC_USAGE)) {
            return;
        }
        mClientImpl.onPageDOMContentLoadedEventFired(
                BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                        new SupportLibWebViewPageAdapter(pageState.getPage())));
    }

    @Override
    public void onFirstContentfulPaint(AwPageState pageState, long loadTimeUs) {
        if (!BoundaryInterfaceReflectionUtil.containsFeature(
                mSupportedFeatures, Features.WEB_VIEW_NAVIGATION_CLIENT_BASIC_USAGE)) {
            return;
        }
        mClientImpl.onFirstContentfulPaint(
                BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                        new SupportLibWebViewPageAdapter(pageState.getPage())));
    }

    // Not implemented as this navigation client is set to be deprecated in favour of
    // {@link #AwNavigationListener}
    @Override
    public void onLargestContentfulPaint(AwPageState pageState, long durationMs) {}

    // Not implemented as this navigation client is set to be deprecated in favour of
    // {@link #AwNavigationListener}
    @Override
    public void onPerformanceMark(AwPageState pageState, String markName, long markNameMs) {}
}
