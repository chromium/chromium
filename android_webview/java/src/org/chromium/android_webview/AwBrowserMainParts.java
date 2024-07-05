// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import org.jni_zero.CalledByNative;

/** Util methods which may be needed by native class of the same name. */
public class AwBrowserMainParts {
    // This is set by WebViewChromiumFactoryProvider for the WebView separate resource context
    // experiment. The value is read by aw_browser_main_parts.cc.
    private static boolean sUseWebViewContext;

    public static void setUseWebViewContext(boolean enabled) {
        sUseWebViewContext = enabled;
    }

    @CalledByNative
    private static boolean getUseWebViewContext() {
        return sUseWebViewContext;
    }

    private AwBrowserMainParts() {}
}
