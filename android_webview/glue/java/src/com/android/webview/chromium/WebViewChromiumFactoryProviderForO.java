// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

class WebViewChromiumFactoryProviderForO extends WebViewChromiumFactoryProvider {
    public static WebViewChromiumFactoryProvider create(android.webkit.WebViewDelegate delegate) {
        return new WebViewChromiumFactoryProviderForO(delegate);
    }

    protected WebViewChromiumFactoryProviderForO(android.webkit.WebViewDelegate delegate) {
        super(delegate);
    }
}
