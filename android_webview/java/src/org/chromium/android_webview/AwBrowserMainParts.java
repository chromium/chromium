// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import org.jni_zero.CalledByNative;

import org.chromium.build.annotations.NullMarked;

/** Util methods which may be needed by native class of the same name. */
@NullMarked
public class AwBrowserMainParts {
    // This is set by WebViewChromiumFactoryProvider for the WebView separate resource context
    // experiment. The value is read by aw_browser_main_parts.cc.
    private static boolean sUseWebViewContext;
    private static boolean sPartitionedCookiesDefaultState;
    private static boolean sWebViewUseStartupTasksLogic;

    public static void setUseWebViewContext(boolean enabled) {
        sUseWebViewContext = enabled;
    }

    public static void setPartitionedCookiesDefaultState(boolean enabled) {
        sPartitionedCookiesDefaultState = enabled;
    }

    public static void setWebViewStartupTasksLogicIsEnabled(boolean enabled) {
        sWebViewUseStartupTasksLogic = enabled;
    }

    @CalledByNative
    private static boolean getUseWebViewContext() {
        return sUseWebViewContext;
    }

    @CalledByNative
    private static boolean getPartitionedCookiesDefaultState() {
        return sPartitionedCookiesDefaultState;
    }

    @CalledByNative
    private static boolean isWebViewStartupTasksLogicEnabled() {
        return sWebViewUseStartupTasksLogic;
    }

    private AwBrowserMainParts() {}
}
