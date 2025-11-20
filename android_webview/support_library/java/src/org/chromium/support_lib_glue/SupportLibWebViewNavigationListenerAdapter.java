// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.support_lib_glue;

import org.chromium.android_webview.AwNavigation;
import org.chromium.android_webview.AwNavigationListener;
import org.chromium.android_webview.AwPage;
import org.chromium.android_webview.common.Lifetime;
import org.chromium.build.annotations.NullMarked;
import org.chromium.support_lib_boundary.WebViewNavigationListenerBoundaryInterface;
import org.chromium.support_lib_boundary.util.BoundaryInterfaceReflectionUtil;
import org.chromium.support_lib_boundary.util.Features;

import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Proxy;
import java.util.concurrent.Executor;

/** Support library glue navigation listener callback adapter. */
@Lifetime.Temporary
@NullMarked
class SupportLibWebViewNavigationListenerAdapter implements AwNavigationListener {
    private final WebViewNavigationListenerBoundaryInterface mImpl;
    private final String[] mSupportedFeatures;
    private final Executor mExecutor;

    public SupportLibWebViewNavigationListenerAdapter(
            /* WebViewNavigationListener */ InvocationHandler invocationHandler,
            Executor executor) {
        mImpl =
                BoundaryInterfaceReflectionUtil.castToSuppLibClass(
                        WebViewNavigationListenerBoundaryInterface.class, invocationHandler);
        mSupportedFeatures = mImpl.getSupportedFeatures();
        mExecutor = executor;
    }

    @Override
    public boolean equals(Object obj) {
        if (obj == this) return true;
        if (obj instanceof SupportLibWebViewNavigationListenerAdapter listener) {
            return getSupportLibInvocationHandler()
                    .equals(listener.getSupportLibInvocationHandler());
        }
        return false;
    }

    @Override
    public int hashCode() {
        return getSupportLibInvocationHandler().hashCode();
    }

    @Override
    public /* WebViewNavigationListener */ InvocationHandler getSupportLibInvocationHandler() {
        return Proxy.getInvocationHandler(mImpl);
    }

    @Override
    public void onNavigationStarted(AwNavigation navigation) {
        if (!BoundaryInterfaceReflectionUtil.containsFeature(
                mSupportedFeatures, Features.WEB_VIEW_NAVIGATION_LISTENER_V1)) {
            return;
        }
        mExecutor.execute(
                () ->
                        mImpl.onNavigationStarted(
                                BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                                        new SupportLibWebViewNavigationAdapter(navigation))));
    }

    @Override
    public void onNavigationRedirected(AwNavigation navigation) {
        if (!BoundaryInterfaceReflectionUtil.containsFeature(
                mSupportedFeatures, Features.WEB_VIEW_NAVIGATION_LISTENER_V1)) {
            return;
        }
        mExecutor.execute(
                () ->
                        mImpl.onNavigationRedirected(
                                BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                                        new SupportLibWebViewNavigationAdapter(navigation))));
    }

    @Override
    public void onNavigationCompleted(AwNavigation navigation) {
        if (!BoundaryInterfaceReflectionUtil.containsFeature(
                mSupportedFeatures, Features.WEB_VIEW_NAVIGATION_LISTENER_V1)) {
            return;
        }
        mExecutor.execute(
                () ->
                        mImpl.onNavigationCompleted(
                                BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                                        new SupportLibWebViewNavigationAdapter(navigation))));
    }

    @Override
    public void onPageDeleted(AwPage page) {
        if (!BoundaryInterfaceReflectionUtil.containsFeature(
                mSupportedFeatures, Features.WEB_VIEW_NAVIGATION_LISTENER_V1)) {
            return;
        }
        mExecutor.execute(
                () ->
                        mImpl.onPageDeleted(
                                BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                                        new SupportLibWebViewPageAdapter(page))));
    }

    @Override
    public void onPageLoadEventFired(AwPage page) {
        if (!BoundaryInterfaceReflectionUtil.containsFeature(
                mSupportedFeatures, Features.WEB_VIEW_NAVIGATION_LISTENER_V1)) {
            return;
        }
        mExecutor.execute(
                () ->
                        mImpl.onPageLoadEventFired(
                                BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                                        new SupportLibWebViewPageAdapter(page))));
    }

    @Override
    public void onPageDOMContentLoadedEventFired(AwPage page) {
        if (!BoundaryInterfaceReflectionUtil.containsFeature(
                mSupportedFeatures, Features.WEB_VIEW_NAVIGATION_LISTENER_V1)) {
            return;
        }
        mExecutor.execute(
                () ->
                        mImpl.onPageDOMContentLoadedEventFired(
                                BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                                        new SupportLibWebViewPageAdapter(page))));
    }

    @Override
    public void onFirstContentfulPaint(AwPage page, long loadTimeUs) {
        if (!BoundaryInterfaceReflectionUtil.containsFeature(
                mSupportedFeatures, Features.WEB_VIEW_NAVIGATION_LISTENER_V1)) {
            return;
        }
        mExecutor.execute(
                () ->
                        mImpl.onFirstContentfulPaint(
                                BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                                        new SupportLibWebViewPageAdapter(page)),
                                loadTimeUs));
    }

    // TODO: crbug.com/432696062 - Implement AndroidX methods
    @Override
    public void onPerformanceMark(AwPage page, String markName, long markNameMs) {}
}
