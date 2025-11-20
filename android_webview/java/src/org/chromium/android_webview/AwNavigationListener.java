// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import org.chromium.android_webview.common.Lifetime;
import org.chromium.build.annotations.NullMarked;

import java.lang.reflect.InvocationHandler;

/** Base-class that an AwContents embedder derives from to receive navigation-related callbacks. */
@Lifetime.WebView
@NullMarked
public interface AwNavigationListener {
    /* WebViewNavigationClient */ InvocationHandler getSupportLibInvocationHandler();

    void onNavigationStarted(AwNavigation navigation);

    void onNavigationRedirected(AwNavigation navigation);

    void onNavigationCompleted(AwNavigation navigation);

    void onPageDeleted(AwPage page);

    void onPageLoadEventFired(AwPage page);

    void onPageDOMContentLoadedEventFired(AwPage page);

    void onFirstContentfulPaint(AwPage page, long loadTimeUs);

    void onPerformanceMark(AwPage page, String markName, long markNameMs);
}
