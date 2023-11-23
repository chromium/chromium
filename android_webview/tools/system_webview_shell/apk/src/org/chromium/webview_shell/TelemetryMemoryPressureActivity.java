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

import org.chromium.base.Log;
import org.chromium.base.MemoryPressureListener;

/** This activity is designed for sending memory pressure signals for testing WebView. */
public class TelemetryMemoryPressureActivity extends Activity {

    private static final String TAG = "WebViewTelemetry";

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        getWindow().setTitle(getResources().getString(R.string.title_activity_telemetry));
        setContentView(R.layout.activity_webview);

        WebView webview = (WebView) findViewById(R.id.webview);
        CookieManager.setAcceptFileSchemeCookies(true);
        webview.getSettings().setJavaScriptEnabled(true);

        webview.setWebViewClient(
                new WebViewClientCompat() {
                    @SuppressWarnings("deprecation") // because we support api level 19 and up.
                    @Override
                    public boolean shouldOverrideUrlLoading(WebView webView, String url) {
                        return false;
                    }
                });

        webview.loadUrl("about:blank");
    }

    @Override
    protected void onNewIntent(Intent intent) {
        moveTaskToBack(true);
        if (MemoryPressureListener.handleDebugIntent(this, intent.getAction())) {
            Log.i(
                    TAG,
                    "MemoryPressureListener.handleDebugIntent(this, "
                            + intent.getAction()
                            + ") is true");
        }
    }
}
