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

    void onNavigationStarted(AwNavigation navigation);

    /**
     * Called when the navigation is redirected.
     *
     * @param navigation The navigation that was redirected.
     * @param responseHeaders The headers of the response that caused the redirect. Note that these
     *     are not the headers of the navigation's final response, which are available from {@link
     *     AwNavigation#getResponseHeaders()} once the navigation has completed.
     * @param statusCode The HTTP status code of the response that caused the redirect. Note that
     *     this is not the status code of the navigation's final response, which is available from
     *     {@link AwNavigation#getStatusCode()} once the navigation has completed.
     */
    void onNavigationRedirected(
            AwNavigation navigation, Map<String, String> responseHeaders, int statusCode);

    void onNavigationCompleted(AwNavigation navigation);

    void onNavigationVisible(AwNavigation navigation);

    void onPageDeleted(AwPage page);

    void onPageLoadEventFired(AwPage page);

    void onPageDOMContentLoadedEventFired(AwPage page);

    void onFirstContentfulPaint(AwPage page, long durationMs);

    void onLargestContentfulPaint(AwPage page, long durationMs);

    void onPerformanceMark(AwPage page, String markName, long markNameMs);
}
