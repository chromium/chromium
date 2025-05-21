// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

/**
 * On Android O, the process of loading WebView expects to find a class with this name.
 *
 * <p>Do not add any new code to this class even if it's OS-version-specific; all logic belongs in
 * the base class, with appropriate SDK_INT checks if needed.
 */
class WebViewChromiumFactoryProviderForO extends WebViewChromiumFactoryProvider {
    public static WebViewChromiumFactoryProvider create(android.webkit.WebViewDelegate delegate) {
        return new WebViewChromiumFactoryProviderForO(delegate);
    }

    protected WebViewChromiumFactoryProviderForO(android.webkit.WebViewDelegate delegate) {
        super(delegate);
    }
}
