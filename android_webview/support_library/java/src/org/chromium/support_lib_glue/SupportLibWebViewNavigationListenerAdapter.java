// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.support_lib_glue;

import org.chromium.android_webview.AwNavigation;
import org.chromium.android_webview.AwNavigationListener;
import org.chromium.android_webview.AwNavigationState;
import org.chromium.android_webview.AwPage;
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
        if (BoundaryInterfaceReflectionUtil.containsFeature(
                mSupportedFeatures, Features.NAVIGATION_LISTENER_NAVIGATION_STATE)) {
            mExecutor.execute(
                    () ->
                            mImpl.onNavigationStartedThreadsafe(
                                    BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                                            new SupportLibWebViewNavigationStateAdapter(
                                                    navigationState))));
            return;
        }
        AwNavigation navigation = navigationState.getNavigation();
        mExecutor.execute(
                () ->
                        mImpl.onNavigationStarted(
                                BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                                        new SupportLibWebViewNavigationAdapter(navigation))));
    }

    @Override
    public void onNavigationRedirected(
            AwNavigationState navigationState,
            Map<String, String> responseHeaders,
            int statusCode) {
        if (!BoundaryInterfaceReflectionUtil.containsFeature(
                mSupportedFeatures, Features.WEB_VIEW_NAVIGATION_LISTENER_V1)) {
            return;
        }
        // also requires feature NAVIGATION_GET_RESPONSE_HEADERS, but both features are implemented
        // in the same AndroidX version.
        if (BoundaryInterfaceReflectionUtil.containsFeature(
                mSupportedFeatures, Features.NAVIGATION_LISTENER_NAVIGATION_STATE)) {
            mExecutor.execute(
                    () ->
                            mImpl.onNavigationRedirectedThreadsafe(
                                    BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                                            new SupportLibWebViewNavigationStateAdapter(
                                                    navigationState)),
                                    BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                                            new SupportLibNavigationRedirectParametersAdapter(
                                                    responseHeaders, statusCode))));
            return;
        }
        AwNavigation navigation = navigationState.getNavigation();
        mExecutor.execute(
                () ->
                        mImpl.onNavigationRedirected(
                                BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                                        new SupportLibWebViewNavigationAdapter(navigation))));
    }

    @Override
    public void onNavigationCompleted(AwNavigationState navigationState) {
        if (!BoundaryInterfaceReflectionUtil.containsFeature(
                mSupportedFeatures, Features.WEB_VIEW_NAVIGATION_LISTENER_V1)) {
            return;
        }
        if (BoundaryInterfaceReflectionUtil.containsFeature(
                mSupportedFeatures, Features.NAVIGATION_LISTENER_NAVIGATION_STATE)) {
            mExecutor.execute(
                    () ->
                            mImpl.onNavigationCompletedThreadsafe(
                                    BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                                            new SupportLibWebViewNavigationStateAdapter(
                                                    navigationState))));
            return;
        }
        AwNavigation navigation = navigationState.getNavigation();
        mExecutor.execute(
                () ->
                        mImpl.onNavigationCompleted(
                                BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                                        new SupportLibWebViewNavigationAdapter(navigation))));
    }

    @Override
    public void onNavigationVisible(AwNavigationState navigationState) {
        if (!BoundaryInterfaceReflectionUtil.containsFeature(
                mSupportedFeatures, Features.WEB_VIEW_NAVIGATION_LISTENER_NAVIGATION_VISIBLE)) {
            return;
        }
        if (BoundaryInterfaceReflectionUtil.containsFeature(
                mSupportedFeatures, Features.NAVIGATION_LISTENER_NAVIGATION_STATE)) {
            mExecutor.execute(
                    () ->
                            mImpl.onNavigationVisibleThreadsafe(
                                    BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                                            new SupportLibWebViewNavigationStateAdapter(
                                                    navigationState))));
            return;
        }
        AwNavigation navigation = navigationState.getNavigation();
        mExecutor.execute(
                () ->
                        mImpl.onNavigationVisible(
                                BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                                        new SupportLibWebViewNavigationAdapter(navigation))));
    }

    @Override
    public void onPageDeleted(AwPageState pageState) {
        if (!BoundaryInterfaceReflectionUtil.containsFeature(
                mSupportedFeatures, Features.WEB_VIEW_NAVIGATION_LISTENER_V1)) {
            return;
        }
        if (BoundaryInterfaceReflectionUtil.containsFeature(
                mSupportedFeatures, Features.NAVIGATION_LISTENER_NAVIGATION_STATE)) {
            mExecutor.execute(
                    () ->
                            mImpl.onPageDeletedThreadsafe(
                                    BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                                            new SupportLibWebViewPageStateAdapter(pageState))));
            return;
        }
        AwPage page = pageState.getPage();
        mExecutor.execute(
                () ->
                        mImpl.onPageDeleted(
                                BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                                        new SupportLibWebViewPageAdapter(page))));
    }

    @Override
    public void onPageLoadEventFired(AwPageState pageState) {
        if (!BoundaryInterfaceReflectionUtil.containsFeature(
                mSupportedFeatures, Features.WEB_VIEW_NAVIGATION_LISTENER_V1)) {
            return;
        }
        if (BoundaryInterfaceReflectionUtil.containsFeature(
                mSupportedFeatures, Features.NAVIGATION_LISTENER_NAVIGATION_STATE)) {
            mExecutor.execute(
                    () ->
                            mImpl.onPageLoadEventFiredThreadsafe(
                                    BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                                            new SupportLibWebViewPageStateAdapter(pageState))));
            return;
        }
        AwPage page = pageState.getPage();
        mExecutor.execute(
                () ->
                        mImpl.onPageLoadEventFired(
                                BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                                        new SupportLibWebViewPageAdapter(page))));
    }

    @Override
    public void onPageDOMContentLoadedEventFired(AwPageState pageState) {
        if (!BoundaryInterfaceReflectionUtil.containsFeature(
                mSupportedFeatures, Features.WEB_VIEW_NAVIGATION_LISTENER_V1)) {
            return;
        }
        if (BoundaryInterfaceReflectionUtil.containsFeature(
                mSupportedFeatures, Features.NAVIGATION_LISTENER_NAVIGATION_STATE)) {
            mExecutor.execute(
                    () ->
                            mImpl.onPageDOMContentLoadedEventFiredThreadsafe(
                                    BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                                            new SupportLibWebViewPageStateAdapter(pageState))));
            return;
        }
        AwPage page = pageState.getPage();
        mExecutor.execute(
                () ->
                        mImpl.onPageDOMContentLoadedEventFired(
                                BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                                        new SupportLibWebViewPageAdapter(page))));
    }

    @Override
    public void onFirstContentfulPaint(AwPageState pageState, long durationMillis) {
        AwPage page = pageState.getPage();
        if (BoundaryInterfaceReflectionUtil.containsFeature(
                mSupportedFeatures, Features.WEB_VIEW_NAVIGATION_LISTENER_V1)) {
            mExecutor.execute(
                    () ->
                            mImpl.onFirstContentfulPaint(
                                    BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                                            new SupportLibWebViewPageAdapter(page)),
                                    TimeUnit.MILLISECONDS.toMicros(durationMillis)));
        }

        if (!BoundaryInterfaceReflectionUtil.containsFeature(
                mSupportedFeatures, Features.WEB_VIEW_NAVIGATION_LISTENER_V2)) {
            return;
        }

        if (BoundaryInterfaceReflectionUtil.containsFeature(
                mSupportedFeatures, Features.NAVIGATION_LISTENER_NAVIGATION_STATE)) {
            mExecutor.execute(
                    () ->
                            mImpl.onFirstContentfulPaintMillisThreadsafe(
                                    BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                                            new SupportLibWebViewPageStateAdapter(pageState)),
                                    durationMillis));
            return;
        }
        mExecutor.execute(
                () ->
                        mImpl.onFirstContentfulPaintMillis(
                                BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                                        new SupportLibWebViewPageAdapter(page)),
                                durationMillis));
    }

    @Override
    public void onLargestContentfulPaint(AwPageState pageState, long durationMillis) {
        if (!BoundaryInterfaceReflectionUtil.containsFeature(
                mSupportedFeatures, Features.WEB_VIEW_NAVIGATION_LISTENER_V2)) {
            return;
        }
        if (BoundaryInterfaceReflectionUtil.containsFeature(
                mSupportedFeatures, Features.NAVIGATION_LISTENER_NAVIGATION_STATE)) {
            mExecutor.execute(
                    () ->
                            mImpl.onLargestContentfulPaintMillisThreadsafe(
                                    BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                                            new SupportLibWebViewPageStateAdapter(pageState)),
                                    durationMillis));
            return;
        }
        AwPage page = pageState.getPage();
        mExecutor.execute(
                () ->
                        mImpl.onLargestContentfulPaintMillis(
                                BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                                        new SupportLibWebViewPageAdapter(page)),
                                durationMillis));
    }

    @Override
    public void onPerformanceMark(AwPageState pageState, String markName, long markTimeMillis) {
        if (!BoundaryInterfaceReflectionUtil.containsFeature(
                mSupportedFeatures, Features.WEB_VIEW_NAVIGATION_LISTENER_V2)) {
            return;
        }
        if (BoundaryInterfaceReflectionUtil.containsFeature(
                mSupportedFeatures, Features.NAVIGATION_LISTENER_NAVIGATION_STATE)) {
            mExecutor.execute(
                    () ->
                            mImpl.onPerformanceMarkMillisThreadsafe(
                                    BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                                            new SupportLibWebViewPageStateAdapter(pageState)),
                                    markName,
                                    markTimeMillis));
            return;
        }
        AwPage page = pageState.getPage();
        mExecutor.execute(
                () ->
                        mImpl.onPerformanceMarkMillis(
                                BoundaryInterfaceReflectionUtil.createInvocationHandlerFor(
                                        new SupportLibWebViewPageAdapter(page)),
                                markName,
                                markTimeMillis));
    }
}
