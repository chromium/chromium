// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.support_lib_glue;

import org.chromium.android_webview.AwNavigationListener;
import org.chromium.android_webview.AwNavigationState;
import org.chromium.android_webview.AwPageState;
import org.chromium.android_webview.common.Lifetime;
import org.chromium.build.annotations.NullMarked;
import org.chromium.support_lib_boundary.WebViewNavigationListenerBoundaryInterface;
import org.chromium.support_lib_boundary.util.BoundaryInterfaceReflectionUtil;
import org.chromium.support_lib_boundary.util.Features;

import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Proxy;
import java.util.Map;
import java.util.concurrent.Executor;
import java.util.concurrent.TimeUnit;

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
    public void onNavigationStarted(AwNavigationState navigationState) {
        if (!BoundaryInterfaceReflectionUtil.containsFeature(
                mSupportedFeatures, Features.WEB_VIEW_NAVIGATION_LISTENER_V1)) {
            return;
        }
        mExecutor.execute(
                () ->
                        mImpl.onNavigationStarted(
                                BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                                        new SupportLibWebViewNavigationAdapter(
                                                navigationState.getNavigation()))));
    }

    @Override
    public void onNavigationRedirected(
            AwNavigationState navigationState,
            Map<String, String> responseHeaders,
            int statusCode) {
        if (BoundaryInterfaceReflectionUtil.containsFeature(
                mSupportedFeatures, Features.NAVIGATION_GET_RESPONSE_HEADERS)) {
            mExecutor.execute(
                    () ->
                            mImpl.onNavigationRedirected(
                                    BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                                            new SupportLibWebViewNavigationAdapter(
                                                    navigationState.getNavigation())),
                                    BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                                            new SupportLibNavigationRedirectParametersAdapter(
                                                    responseHeaders, statusCode))));
            return;
        }
        if (!BoundaryInterfaceReflectionUtil.containsFeature(
                mSupportedFeatures, Features.WEB_VIEW_NAVIGATION_LISTENER_V1)) {
            return;
        }
        mExecutor.execute(
                () ->
                        mImpl.onNavigationRedirected(
                                BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                                        new SupportLibWebViewNavigationAdapter(
                                                navigationState.getNavigation()))));
    }

    @Override
    public void onNavigationCompleted(AwNavigationState navigationState) {
        if (!BoundaryInterfaceReflectionUtil.containsFeature(
                mSupportedFeatures, Features.WEB_VIEW_NAVIGATION_LISTENER_V1)) {
            return;
        }
        mExecutor.execute(
                () ->
                        mImpl.onNavigationCompleted(
                                BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                                        new SupportLibWebViewNavigationAdapter(
                                                navigationState.getNavigation()))));
    }

    @Override
    public void onNavigationVisible(AwNavigationState navigationState) {
        if (!BoundaryInterfaceReflectionUtil.containsFeature(
                mSupportedFeatures, Features.WEB_VIEW_NAVIGATION_LISTENER_NAVIGATION_VISIBLE)) {
            return;
        }
        mExecutor.execute(
                () ->
                        mImpl.onNavigationVisible(
                                BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                                        new SupportLibWebViewNavigationAdapter(
                                                navigationState.getNavigation()))));
    }

    @Override
    public void onPageDeleted(AwPageState pageState) {
        if (!BoundaryInterfaceReflectionUtil.containsFeature(
                mSupportedFeatures, Features.WEB_VIEW_NAVIGATION_LISTENER_V1)) {
            return;
        }
        mExecutor.execute(
                () ->
                        mImpl.onPageDeleted(
                                BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                                        new SupportLibWebViewPageAdapter(pageState.getPage()))));
    }

    @Override
    public void onPageLoadEventFired(AwPageState pageState) {
        if (!BoundaryInterfaceReflectionUtil.containsFeature(
                mSupportedFeatures, Features.WEB_VIEW_NAVIGATION_LISTENER_V1)) {
            return;
        }
        mExecutor.execute(
                () ->
                        mImpl.onPageLoadEventFired(
                                BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                                        new SupportLibWebViewPageAdapter(pageState.getPage()))));
    }

    @Override
    public void onPageDOMContentLoadedEventFired(AwPageState pageState) {
        if (!BoundaryInterfaceReflectionUtil.containsFeature(
                mSupportedFeatures, Features.WEB_VIEW_NAVIGATION_LISTENER_V1)) {
            return;
        }
        mExecutor.execute(
                () ->
                        mImpl.onPageDOMContentLoadedEventFired(
                                BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                                        new SupportLibWebViewPageAdapter(pageState.getPage()))));
    }

    @Override
    public void onFirstContentfulPaint(AwPageState pageState, long durationMillis) {
        if (BoundaryInterfaceReflectionUtil.containsFeature(
                mSupportedFeatures, Features.WEB_VIEW_NAVIGATION_LISTENER_V1)) {
            mExecutor.execute(
                    () ->
                            mImpl.onFirstContentfulPaint(
                                    BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                                            new SupportLibWebViewPageAdapter(pageState.getPage())),
                                    TimeUnit.MILLISECONDS.toMicros(durationMillis)));
        }

        if (BoundaryInterfaceReflectionUtil.containsFeature(
                mSupportedFeatures, Features.WEB_VIEW_NAVIGATION_LISTENER_V2)) {
            mExecutor.execute(
                    () ->
                            mImpl.onFirstContentfulPaintMillis(
                                    BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                                            new SupportLibWebViewPageAdapter(pageState.getPage())),
                                    durationMillis));
        }
    }

    @Override
    public void onLargestContentfulPaint(AwPageState pageState, long durationMillis) {
        if (!BoundaryInterfaceReflectionUtil.containsFeature(
                mSupportedFeatures, Features.WEB_VIEW_NAVIGATION_LISTENER_V2)) {
            return;
        }
        mExecutor.execute(
                () ->
                        mImpl.onLargestContentfulPaintMillis(
                                BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                                        new SupportLibWebViewPageAdapter(pageState.getPage())),
                                durationMillis));
    }

    @Override
    public void onPerformanceMark(AwPageState pageState, String markName, long markTimeMillis) {
        if (!BoundaryInterfaceReflectionUtil.containsFeature(
                mSupportedFeatures, Features.WEB_VIEW_NAVIGATION_LISTENER_V2)) {
            return;
        }
        mExecutor.execute(
                () ->
                        mImpl.onPerformanceMarkMillis(
                                BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                                        new SupportLibWebViewPageAdapter(pageState.getPage())),
                                markName,
                                markTimeMillis));
    }
}
