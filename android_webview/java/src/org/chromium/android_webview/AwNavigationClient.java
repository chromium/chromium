// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import org.chromium.android_webview.common.Lifetime;

import java.lang.reflect.InvocationHandler;

/** Base-class that an AwContents embedder derives from to receive navigation-related callbacks. */
@Lifetime.WebView
public interface AwNavigationClient {
    public /* WebViewNavigationClient */ InvocationHandler getSupportLibInvocationHandler();

    public abstract void onNavigationStarted(AwNavigation navigation);

    public abstract void onNavigationRedirected(AwNavigation navigation);

    public abstract void onNavigationCompleted(AwNavigation navigation);

    public abstract void onPageDeleted(AwPage page);

    public abstract void onPageLoadEventFired(AwPage page);

    public abstract void onPageDOMContentLoadedEventFired(AwPage page);

    public abstract void onFirstContentfulPaint(AwPage page);
}
