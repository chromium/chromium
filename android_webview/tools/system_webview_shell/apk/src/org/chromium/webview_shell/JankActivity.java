// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webview_shell;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.webkit.CookieManager;
import android.webkit.WebView;

import androidx.webkit.WebViewClientCompat;

/**
 * This activity is designed for Android Jank testing of WebView. It takes a URL as an argument, and
 * displays the page ready for the Jank tester to test scrolling etc.
 */
public class JankActivity extends Activity {

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        getWindow().setTitle(getResources().getString(R.string.title_activity_jank));
        setContentView(R.layout.activity_webview);

        WebView webView = (WebView) findViewById(R.id.webview);
        CookieManager.setAcceptFileSchemeCookies(true);

        webView.setWebViewClient(
                new WebViewClientCompat() {
                    @SuppressWarnings("deprecation") // because we support api level 19 and up.
                    @Override
                    public boolean shouldOverrideUrlLoading(WebView webView, String url) {
                        return false;
                    }
                });

        String url = getUrlFromIntent(getIntent());
        webView.loadUrl(url);
    }

    private static String getUrlFromIntent(Intent intent) {
        return intent != null ? intent.getDataString() : null;
    }
}
