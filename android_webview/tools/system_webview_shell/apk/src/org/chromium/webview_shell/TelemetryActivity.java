// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.webview_shell;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.os.Trace;
import android.webkit.CookieManager;
import android.webkit.WebSettings;
import android.webkit.WebView;

import androidx.webkit.WebViewClientCompat;

/** This activity is designed for Telemetry testing of WebView. */
public class TelemetryActivity extends Activity {
    static final String START_UP_TRACE_TAG_NAME = "WebViewStartUpTraceTag";
    static final String DEFAULT_START_UP_TRACE_TAG = "WebViewStartupInterval";

    static final String LOAD_URL_TRACE_TAG_NAME = "WebViewLoadUrlTraceTag";
    static final String DEFAULT_LOAD_URL_TRACE_TAG = "WebViewBlankUrlLoadInterval";

    static final String START_UP_AND_LOAD_URL_TRACE_TAG_NAME = "WebViewStartUpAndLoadUrlTraceTag";
    static final String DEFAULT_START_UP_AND_LOAD_URL_TRACE_TAG =
            "WebViewStartupAndLoadBlankUrlInterval";

    static final String PLACEHOLDER_TRACE_TAG_NAME = "WebViewDummyTraceTag";
    static final String DEFAULT_PLACEHOLDER_TRACE_TAG = "WebViewDummyInterval";

    private Intent mIntent;

    private String getTraceTag(String tagName, String tagDefault) {
        String tag = mIntent.getStringExtra(tagName);
        return tag == null ? tagDefault : tag;
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mIntent = getIntent();

        getWindow().setTitle(getResources().getString(R.string.title_activity_telemetry));

        final String startUpTraceTag =
                getTraceTag(START_UP_TRACE_TAG_NAME, DEFAULT_START_UP_TRACE_TAG);
        final String loadUrlTraceTag =
                getTraceTag(LOAD_URL_TRACE_TAG_NAME, DEFAULT_LOAD_URL_TRACE_TAG);
        final String startUpAndLoadUrlTraceTag =
                getTraceTag(
                        START_UP_AND_LOAD_URL_TRACE_TAG_NAME,
                        DEFAULT_START_UP_AND_LOAD_URL_TRACE_TAG);
        final String placeholderTraceTag =
                getTraceTag(PLACEHOLDER_TRACE_TAG_NAME, DEFAULT_PLACEHOLDER_TRACE_TAG);

        Trace.beginSection(startUpAndLoadUrlTraceTag);
        Trace.beginSection(startUpTraceTag);
        WebView webView = new WebView(this);
        setContentView(webView);
        Trace.endSection();

        CookieManager.setAcceptFileSchemeCookies(true);
        WebSettings settings = webView.getSettings();
        settings.setBuiltInZoomControls(true);
        settings.setDisplayZoomControls(false);
        settings.setJavaScriptEnabled(true);
        settings.setUseWideViewPort(true);
        settings.setLoadWithOverviewMode(true);
        settings.setDomStorageEnabled(true);
        settings.setMediaPlaybackRequiresUserGesture(false);
        String userAgentString = mIntent.getStringExtra("userAgent");
        if (userAgentString != null) {
            settings.setUserAgentString(userAgentString);
        }

        webView.setWebViewClient(
                new WebViewClientCompat() {
                    @SuppressWarnings("deprecation") // because we support api level 19 and up.
                    @Override
                    public boolean shouldOverrideUrlLoading(WebView view, String url) {
                        return false;
                    }

                    @Override
                    public void onPageFinished(WebView view, String url) {
                        super.onPageFinished(view, url);
                        // placeholderTraceTag was ended by code in Android intended to end
                        // activityStart before onPageFinished was called, so the time
                        // reported as the activityStart will be longer than actual.
                        // The actual duration of activityStart will be from the
                        // beginning of the activityStart to the end of the placeholderTraceTag

                        // Ends loadUrlTraceTag
                        Trace.endSection();

                        // Ends startUpAndLoadUrlTraceTag
                        Trace.endSection();

                        // Ends activityStart
                        Trace.endSection();
                    }
                });

        Trace.beginSection(loadUrlTraceTag);

        webView.loadUrl("about:blank");

        // TODO(aluo): Use async tracing to avoid having to do this
        // placeholderTraceTag is needed here to prevent code in Android intended to
        // end activityStart from ending loadUrlTraceTag prematurely,
        // see crbug/919221
        Trace.beginSection(placeholderTraceTag);
    }
}
