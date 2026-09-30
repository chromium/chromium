// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import org.chromium.android_webview.common.Lifetime;
import org.chromium.build.annotations.NullMarked;

import java.lang.reflect.InvocationHandler;
import java.util.Map;

/** Base-class that an AwContents embedder derives from to receive navigation-related callbacks. */
@Lifetime.WebView
@NullMarked
public interface AwNavigationListener {
    /* WebViewNavigationClient */ InvocationHandler getSupportLibInvocationHandler();

    void onNavigationStarted(AwNavigationState navigationState);

    /**
     * Called when the navigation is redirected.
     *
     * @param navigationState The NavigationState of the navigation that was redirected.
     * @param responseHeaders The headers of the response that caused the redirect. Note that these
     *     are not the headers of the navigation's final response, which are available from {@link
     *     AwNavigation#getResponseHeaders()} once the navigation has completed.
     * @param statusCode The HTTP status code of the response that caused the redirect. Note that
     *     this is not the status code of the navigation's final response, which is available from
     *     {@link AwNavigation#getStatusCode()} once the navigation has completed.
     */
    void onNavigationRedirected(
            AwNavigationState navigationState, Map<String, String> responseHeaders, int statusCode);

    void onNavigationCompleted(AwNavigationState navigationState);

    void onNavigationVisible(AwNavigationState navigationState);

    void onPageDeleted(AwPageState pageState);

    void onPageLoadEventFired(AwPageState pageState);

    void onPageDOMContentLoadedEventFired(AwPageState pageState);

    void onFirstContentfulPaint(AwPageState pageState, long durationMs);

    void onLargestContentfulPaint(AwPageState pageState, long durationMs);

    void onPerformanceMark(AwPageState pageState, String markName, long markNameMs);
}
