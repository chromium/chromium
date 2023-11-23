// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webview_shell;

import android.app.Activity;
import android.os.Bundle;
import android.webkit.WebView;

/**
 * This activity is used for loading urls using webview on Appurify bots to make sure the
 * webview doesn't crash.
 */
public class PageCyclerTestActivity extends Activity {

    private WebView mWebView;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_webview);
        mWebView = (WebView) findViewById(R.id.webview);
    }

    public WebView getWebView() {
        return mWebView;
    }
}
