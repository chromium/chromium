// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.support_lib_glue;

import org.chromium.android_webview.AwNavigation;
import org.chromium.android_webview.AwNavigationListener;
import org.chromium.android_webview.AwPage;
import org.chromium.android_webview.common.Lifetime;
import org.chromium.support_lib_boundary.WebViewNavigationClientBoundaryInterface;
import org.chromium.support_lib_boundary.util.BoundaryInterfaceReflectionUtil;
import org.chromium.support_lib_boundary.util.Features;

import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Proxy;

/**
 * Support library glue navigation client callback dapter.
 *
 * <p>A new instance of this class is created transiently for every shared library WebViewCompat
 * call. Do not store state here.
 */
@Lifetime.Temporary
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
    public void onNavigationStarted(AwNavigation navigation) {
        if (!BoundaryInterfaceReflectionUtil.containsFeature(
                mSupportedFeatures, Features.WEB_VIEW_NAVIGATION_CLIENT_BASIC_USAGE)) {
            return;
        }
        mClientImpl.onNavigationStarted(
                BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                        new SupportLibWebViewNavigationAdapter(navigation)));
    }

    @Override
    public void onNavigationRedirected(AwNavigation navigation) {
        if (!BoundaryInterfaceReflectionUtil.containsFeature(
                mSupportedFeatures, Features.WEB_VIEW_NAVIGATION_CLIENT_BASIC_USAGE)) {
            return;
        }
        mClientImpl.onNavigationRedirected(
                BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                        new SupportLibWebViewNavigationAdapter(navigation)));
    }

    @Override
    public void onNavigationCompleted(AwNavigation navigation) {
        if (!BoundaryInterfaceReflectionUtil.containsFeature(
                mSupportedFeatures, Features.WEB_VIEW_NAVIGATION_CLIENT_BASIC_USAGE)) {
            return;
        }
        mClientImpl.onNavigationCompleted(
                BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                        new SupportLibWebViewNavigationAdapter(navigation)));
    }

    @Override
    public void onPageDeleted(AwPage page) {
        if (!BoundaryInterfaceReflectionUtil.containsFeature(
                mSupportedFeatures, Features.WEB_VIEW_NAVIGATION_CLIENT_BASIC_USAGE)) {
            return;
        }
        mClientImpl.onPageDeleted(
                BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                        new SupportLibWebViewPageAdapter(page)));
    }

    @Override
    public void onPageLoadEventFired(AwPage page) {
        if (!BoundaryInterfaceReflectionUtil.containsFeature(
                mSupportedFeatures, Features.WEB_VIEW_NAVIGATION_CLIENT_BASIC_USAGE)) {
            return;
        }
        mClientImpl.onPageLoadEventFired(
                BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                        new SupportLibWebViewPageAdapter(page)));
    }

    @Override
    public void onPageDOMContentLoadedEventFired(AwPage page) {
        if (!BoundaryInterfaceReflectionUtil.containsFeature(
                mSupportedFeatures, Features.WEB_VIEW_NAVIGATION_CLIENT_BASIC_USAGE)) {
            return;
        }
        mClientImpl.onPageDOMContentLoadedEventFired(
                BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                        new SupportLibWebViewPageAdapter(page)));
    }

    @Override
    public void onFirstContentfulPaint(AwPage page, long loadTimeUs) {
        if (!BoundaryInterfaceReflectionUtil.containsFeature(
                mSupportedFeatures, Features.WEB_VIEW_NAVIGATION_CLIENT_BASIC_USAGE)) {
            return;
        }
        mClientImpl.onFirstContentfulPaint(
                BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                        new SupportLibWebViewPageAdapter(page)));
    }

    // TODO: crbug.com/432696062 - Implement AndroidX methods
    @Override
    public void onPerformanceMark(AwPage page, String markName, long markNameMs) {}
}
