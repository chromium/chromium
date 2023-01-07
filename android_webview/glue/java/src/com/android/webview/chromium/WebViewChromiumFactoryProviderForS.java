// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

class WebViewChromiumFactoryProviderForS extends WebViewChromiumFactoryProvider {
    public static WebViewChromiumFactoryProvider create(android.webkit.WebViewDelegate delegate) {
        return new WebViewChromiumFactoryProviderForS(delegate);
    }

    protected WebViewChromiumFactoryProviderForS(android.webkit.WebViewDelegate delegate) {
        super(delegate);
    }
}
