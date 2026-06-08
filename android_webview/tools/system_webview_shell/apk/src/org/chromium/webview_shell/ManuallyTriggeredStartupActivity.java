// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webview_shell;

/**
 * WebViewBrowserActivity that waits for a button press on startup, and then again
 * in the fragment before initializing WebView.
 */
public class ManuallyTriggeredStartupActivity extends WebViewBrowserActivity {
    @Override
    protected boolean shouldDelayStartup() {
        return true;
    }

    @Override
    protected String getBrowserToolbarTitle() {
        return getResources().getString(R.string.title_activity_manually_triggered_startup);
    }
}
