// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webview_shell;

import android.os.Bundle;
import android.webkit.WebChromeClient;
import android.webkit.WebView;

import androidx.activity.OnBackPressedCallback;
import androidx.appcompat.app.AppCompatActivity;
import androidx.webkit.WebViewClientCompat;

import java.util.Objects;

/** Activity to exercise insets in WebView when fullscreen. */
public class FullscreenActivity extends AppCompatActivity {
    public static final String URL_EXTRA = "WebViewURL";

    private WebView mWebView;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        EdgeToEdgeUtil.setupEdgeToEdgeFullscreen(this);
        setContentView(R.layout.activity_webview);
        mWebView = findViewById(R.id.webview);
        initializeWebView();
        String url = getIntent().getStringExtra(URL_EXTRA);
        mWebView.loadUrl(Objects.requireNonNullElse(url, "about:blank"));
        registerBackPressedCallback();
    }

    private void initializeWebView() {
        mWebView.setWebViewClient(new WebViewClientCompat());
        mWebView.setWebChromeClient(new WebChromeClient());
        WebViewBrowserFragment.initializeSettings(mWebView.getSettings(), this);
    }

    private void registerBackPressedCallback() {
        getOnBackPressedDispatcher()
                .addCallback(
                        new OnBackPressedCallback(true) {
                            @Override
                            public void handleOnBackPressed() {
                                finish();
                            }
                        });
    }
}
