// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webview_shell;

import android.os.Build;
import android.os.Bundle;
import android.webkit.WebView;

/**
 * Duplicate of WebViewBrowserActivity that's configured to run in a different process,
 * to test what happens when multiple processes use WebView.
 */
public class WebViewBrowserSecondProcessActivity extends WebViewBrowserActivity {
    private static boolean sDataDirSet;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        if (!sDataDirSet && Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            WebView.setDataDirectorySuffix("second_process");
            sDataDirSet = true;
        }
        super.onCreate(savedInstanceState);
    }
}
