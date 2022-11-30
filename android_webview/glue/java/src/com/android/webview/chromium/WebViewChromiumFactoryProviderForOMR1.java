// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

class WebViewChromiumFactoryProviderForOMR1 extends WebViewChromiumFactoryProvider {
    public static WebViewChromiumFactoryProvider create(android.webkit.WebViewDelegate delegate) {
        return new WebViewChromiumFactoryProviderForOMR1(delegate);
    }

    protected WebViewChromiumFactoryProviderForOMR1(android.webkit.WebViewDelegate delegate) {
        super(delegate);
    }
}
