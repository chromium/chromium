// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import android.webkit.WebViewDelegate;

import org.chromium.android_webview.common.Lifetime;

/**
 * On Android T and later, the process of loading WebView expects to find a class with this name.
 *
 * <p>There is no specific factory provider class for U, V, or B because we did not make any changes
 * to the API between android.webkit and the WebView implementation in those OS versions.
 *
 * <p>For OS versions after B, we no longer change the class name even when there are changes to the
 * API. In C we used conditional compilation to select a downstream version of this class, but this
 * mechanism has now been removed as it didn't work as well as expected.
 *
 * <p>TODO(b/553990254): find a replacement mechanism, if it turns out to be needed.
 *
 * <p>Do not add any new code to this class even if it's OS-version-specific; all logic belongs in
 * the base class, with appropriate SDK_INT checks if needed.
 */
@Lifetime.Singleton
class WebViewChromiumFactoryProviderForT extends WebViewChromiumFactoryProvider {
    public static WebViewChromiumFactoryProvider create(WebViewDelegate delegate) {
        return WebViewChromiumFactoryProvider.create(delegate);
    }

    protected WebViewChromiumFactoryProviderForT(WebViewDelegate delegate) {
        super(delegate);
    }
}
