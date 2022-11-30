// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

public class WebViewChromiumFactoryProviderForP extends WebViewChromiumFactoryProvider {
    public static WebViewChromiumFactoryProvider create(android.webkit.WebViewDelegate delegate) {
        return new WebViewChromiumFactoryProviderForP(delegate);
    }

    protected WebViewChromiumFactoryProviderForP(android.webkit.WebViewDelegate delegate) {
        super(delegate);
    }
}
