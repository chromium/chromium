// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

/**
 * Contains command line switches that are specific to Android WebView.
 */
public final class AwSwitches {
    // Allow mirroring JS console messages to system logs.
    // Native switch kWebViewLogJsConsoleMessages.
    public static final String WEBVIEW_LOG_JS_CONSOLE_MESSAGES = "webview-log-js-console-messages";

    // Indicate that renderers are running in a sandbox. Enables
    // kInProcessGPU and sets kRendererProcessLimit to 1.
    // Native switch kWebViewSandboxedRenderer.
    public static final String WEBVIEW_SANDBOXED_RENDERER = "webview-sandboxed-renderer";

    // Disables safebrowsing functionality in webview.
    // Native switch kWebViewDisableSafeBrowsingSupport.
    public static final String WEBVIEW_DISABLE_SAFEBROWSING_SUPPORT =
            "webview-disable-safebrowsing-support";

    // Enables SafeBrowsing and causes WebView to treat all resources as malicious. Use care: this
    // will block all resources from loading.
    // No native switch.
    public static final String WEBVIEW_SAFEBROWSING_BLOCK_ALL_RESOURCES =
            "webview-safebrowsing-block-all-resources";

    // Do not instantiate this class.
    private AwSwitches() {}
}
