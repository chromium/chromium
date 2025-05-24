// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

/**
 * On Android R, the process of loading WebView expects to find a class with this name.
 *
 * <p>Do not add any new code to this class even if it's OS-version-specific; all logic belongs in
 * the base class, with appropriate SDK_INT checks if needed.
 */
class WebViewChromiumFactoryProviderForR extends WebViewChromiumFactoryProvider {
    public static WebViewChromiumFactoryProvider create(android.webkit.WebViewDelegate delegate) {
        return new WebViewChromiumFactoryProviderForR(delegate);
    }

    protected WebViewChromiumFactoryProviderForR(android.webkit.WebViewDelegate delegate) {
        super(delegate);
    }
}
