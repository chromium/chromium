// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import org.chromium.android_webview.common.Lifetime;
import org.chromium.build.annotations.UsedByReflection;

@Lifetime.Singleton
class WebViewChromiumFactoryProviderForT extends WebViewChromiumFactoryProvider {
    public static WebViewChromiumFactoryProvider create(android.webkit.WebViewDelegate delegate) {
        return new WebViewChromiumFactoryProviderForT(delegate);
    }

    @UsedByReflection("Samsung KNOX - crbug.com/1462346")
    protected WebViewChromiumFactoryProviderForT(android.webkit.WebViewDelegate delegate) {
        super(delegate);
    }
}
