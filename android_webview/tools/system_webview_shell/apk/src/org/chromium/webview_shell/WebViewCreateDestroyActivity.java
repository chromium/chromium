// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webview_shell;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.view.ViewGroup;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.widget.RelativeLayout;

import androidx.webkit.WebViewClientCompat;

/**
 * This activity always has at most one live webview. Any previously exisisting
 * webview instance is destroyed first before creating a new one. This activity
 * is designed for testing create/destroy webview sequence, for catching potential
 * memory leaks and memory benchmarking.
 *
 * Note that this activity does not destroy any webviews in other activities. For
 * example launching TelemetryActivity followed by WebViewCreateDestroyActivity
 * will yield two webview instances in total.
 */
public class WebViewCreateDestroyActivity extends Activity {
    @SuppressLint("StaticFieldLeak")
    private static WebView sWebView;

    @Override
    protected void onDestroy() {
        destroyWebViewIfExists();
        super.onDestroy();
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_empty);
        getWindow().setTitle(getResources().getString(R.string.title_activity_create_destroy));
        onNewIntent(getIntent());
    }

    @Override
    protected void onNewIntent(Intent intent) {
        destroyWebViewIfExists();
        openUsingNewWebView(intent);
    }

    private void openUsingNewWebView(Intent intent) {
        sWebView = new WebView(this);
        sWebView.setLayoutParams(
                new ViewGroup.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
        RelativeLayout layout = (RelativeLayout) findViewById(R.id.emptyview);
        layout.addView(sWebView);

        WebSettings webSettings = sWebView.getSettings();
        webSettings.setJavaScriptEnabled(true);
        webSettings.setUseWideViewPort(true);
        webSettings.setLoadWithOverviewMode(true);

        sWebView.setWebViewClient(
                new WebViewClientCompat() {
                    @SuppressWarnings("deprecation") // because we support api level 19 and up.
                    @Override
                    public boolean shouldOverrideUrlLoading(WebView view, String url) {
                        return false;
                    }
                });

        String url = getUrlFromIntent(intent);
        sWebView.loadUrl(url == null ? "about:blank" : url);
    }

    private void destroyWebViewIfExists() {
        if (sWebView == null) return;
        RelativeLayout layout = (RelativeLayout) findViewById(R.id.emptyview);
        layout.removeView(sWebView);
        sWebView.destroy();
        sWebView = null;
    }

    private static String getUrlFromIntent(Intent intent) {
        return intent != null ? intent.getDataString() : null;
    }
}
