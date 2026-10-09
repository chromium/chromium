// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_boundary;

import org.jspecify.annotations.NullMarked;

import java.lang.reflect.InvocationHandler;

/** Boundary interface for WebViewNavigationListener. */
@NullMarked
public interface WebViewNavigationListenerBoundaryInterface
        extends FeatureFlagHolderBoundaryInterface {
    void onNavigationStarted(/* WebViewNavigation */ InvocationHandler navigation);

    default void onNavigationStartedThreadsafe(
            /* WebViewNavigationState */ InvocationHandler navigationState) {}

    void onNavigationRedirected(/* WebViewNavigation */ InvocationHandler navigation);

    default void onNavigationRedirectedThreadsafe(
            /* WebViewNavigationState */ InvocationHandler navigationState,
            /* NavigationRedirectParameters */ InvocationHandler redirectParameters) {}

    void onNavigationCompleted(/* WebViewNavigation */ InvocationHandler navigation);

    default void onNavigationCompletedThreadsafe(
            /* WebViewNavigationState */ InvocationHandler navigationState) {}

    default void onNavigationVisible(/* WebViewNavigation */ InvocationHandler navigation) {}

    default void onNavigationVisibleThreadsafe(
            /* WebViewNavigationState */ InvocationHandler navigationState) {}

    void onPageDeleted(/* WebViewPage */ InvocationHandler page);

    default void onPageDeletedThreadsafe(/* WebViewPageState */ InvocationHandler pageState) {}

    void onPageLoadEventFired(/* WebViewPage */ InvocationHandler page);

    default void onPageLoadEventFiredThreadsafe(
            /* WebViewPageState */ InvocationHandler pageState) {}

    void onPageDOMContentLoadedEventFired(/* WebViewPage */ InvocationHandler page);

    default void onPageDOMContentLoadedEventFiredThreadsafe(
            /* WebViewPageState */ InvocationHandler pageState) {}

    void onFirstContentfulPaint(/* WebViewPage */ InvocationHandler page, long loadTimeUs);

    void onFirstContentfulPaintMillis(
            /* WebViewPage */ InvocationHandler page, long durationMillis);

    default void onFirstContentfulPaintMillisThreadsafe(
            /* WebViewPageState */ InvocationHandler pageState, long durationMillis) {}

    void onLargestContentfulPaintMillis(
            /* WebViewPage */ InvocationHandler page, long durationMillis);

    default void onLargestContentfulPaintMillisThreadsafe(
            /* WebViewPageState */ InvocationHandler pageState, long durationMillis) {}

    void onPerformanceMarkMillis(
            /* WebViewPage */ InvocationHandler page, String markName, long durationMillis);

    default void onPerformanceMarkMillisThreadsafe(
            /* WebViewPageState */ InvocationHandler pageState,
            String markName,
            long durationMillis) {}
}
