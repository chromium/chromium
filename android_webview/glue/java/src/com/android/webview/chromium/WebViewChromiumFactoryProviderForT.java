// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import org.chromium.android_webview.common.Lifetime;

/**
 * On Android T and later, the process of loading WebView expects to find a class with this name.
 *
 * <p>There is no specific factory provider class for U, V, or B because we did not make any changes
 * to the API between android.webkit and the WebView implementation in those OS versions.
 *
 * <p>For OS versions after B, we no longer change the class name even when there are changes to the
 * API. Instead, this class is conditionally compiled: when compiling against a public version of
 * the Android SDK, this version (which behaves identically to the base class) is used, but when
 * compiling against an internal development snapshot, a separate copy of the class from
 * //clank/android_webview is used instead, which can refer to new APIs that are not yet included in
 * the released SDK.
 *
 * <p>Do not add any new code to the upstream version of this class; all logic for publicly-released
 * Android OS versions belongs in the base class, with appropriate SDK_INT checks if needed. Logic
 * for unreleased Android OS versions should be implemented in the downstream copy of this class.
 */
@Lifetime.Singleton
class WebViewChromiumFactoryProviderForT extends WebViewChromiumFactoryProvider {
    public static WebViewChromiumFactoryProvider create(android.webkit.WebViewDelegate delegate) {
        return new WebViewChromiumFactoryProviderForT(delegate);
    }

    protected WebViewChromiumFactoryProviderForT(android.webkit.WebViewDelegate delegate) {
        super(delegate);
    }
}
