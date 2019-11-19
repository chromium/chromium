// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

public class WebViewChromiumFactoryProviderForQ extends WebViewChromiumFactoryProvider {
    public static WebViewChromiumFactoryProvider create(android.webkit.WebViewDelegate delegate) {
        return new WebViewChromiumFactoryProviderForQ(delegate);
    }

    protected WebViewChromiumFactoryProviderForQ(android.webkit.WebViewDelegate delegate) {
        super(delegate);
    }
}
