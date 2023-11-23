// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webview_shell;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.os.SystemClock;
import android.webkit.WebSettings;
import android.webkit.WebView;

import androidx.webkit.TracingConfig;
import androidx.webkit.TracingController;
import androidx.webkit.WebViewClientCompat;

import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.concurrent.Executors;

/**
 * This activity is designed for telemetry testing of WebView Tracing API.
 *
 * In particular it allows to measure time for loading a given URL with or without
 * tracing enabled. It also provides the overall tracing time, i.e. time it takes from
 * starting tracing until completion when all traces are written to disk.
 *
 * Example usage:
 * $ adb shell am start -n org.chromium.webview_shell/.WebViewTracingActivity -a VIEW -d \
 *   http://www.google.com --ez enableTracing true
 *
 */
public class WebViewTracingActivity extends Activity {
    private static final String TAG = "WebViewTracingActivity";

    private static long sUrlLoadTimeStartMs;
    private static long sUrlLoadTimeEndMs;

    private static long sOverallTracingTimeStartMs;
    private static long sOverallTracingTimeEndMs;

    private static class TracingLogger extends FileOutputStream {
        private long mByteCount;
        private Activity mActivity;

        public TracingLogger(String fileName, Activity activity) throws FileNotFoundException {
            super(fileName);
            mActivity = activity;
        }

        @Override
        public void write(byte[] chunk) throws IOException {
            mByteCount += chunk.length;
            super.write(chunk);
        }

        @Override
        public void close() throws IOException {
            super.close();
            sOverallTracingTimeEndMs = SystemClock.elapsedRealtime();
            android.util.Log.i(
                    TAG,
                    "TracingLogger.complete(), byte count = "
                            + getByteCount()
                            + ", overall duration (ms) = "
                            + (sOverallTracingTimeEndMs - sOverallTracingTimeStartMs));
            mActivity.finish();
        }

        public long getByteCount() {
            return mByteCount;
        }
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        getWindow().setTitle("WebViewTracingActivity");

        Intent intent = getIntent();
        String url = getUrlFromIntent(intent);
        if (url == null) {
            url = "about:blank";
        }

        boolean enableTracing = false;
        if (intent.getExtras() != null) {
            enableTracing = intent.getExtras().getBoolean("enableTracing", false);
        }

        loadUrl(url, enableTracing);
    }

    private void loadUrl(final String url, boolean enableTracing) {
        final Activity activity = this;
        WebView webView = new WebView(this);
        setContentView(webView);
        WebSettings settings = webView.getSettings();
        settings.setJavaScriptEnabled(true);
        final TracingController tracingController = TracingController.getInstance();

        webView.setWebViewClient(
                new WebViewClientCompat() {
                    @Override
                    public void onPageFinished(WebView view, String url) {
                        super.onPageFinished(view, url);
                        sUrlLoadTimeEndMs = SystemClock.elapsedRealtime();
                        android.util.Log.i(
                                TAG,
                                "onPageFinished(), enableTracing = "
                                        + enableTracing
                                        + ", url = "
                                        + url
                                        + ", urlLoadTimeMillis = "
                                        + (sUrlLoadTimeEndMs - sUrlLoadTimeStartMs));

                        if (enableTracing) {
                            String outFileName = getFilesDir() + "/webview_tracing.json";
                            try {
                                tracingController.stop(
                                        new TracingLogger(outFileName, activity),
                                        Executors.newSingleThreadExecutor());
                            } catch (FileNotFoundException e) {
                                throw new RuntimeException(e);
                            }
                        } else {
                            activity.finish();
                        }
                    }
                });

        if (enableTracing) {
            sOverallTracingTimeStartMs = SystemClock.elapsedRealtime();
            tracingController.start(
                    new TracingConfig.Builder()
                            .addCategories(TracingConfig.CATEGORIES_WEB_DEVELOPER)
                            .setTracingMode(TracingConfig.RECORD_UNTIL_FULL)
                            .build());
        }
        sUrlLoadTimeStartMs = SystemClock.elapsedRealtime();
        webView.loadUrl(url);
    }

    private static String getUrlFromIntent(Intent intent) {
        return intent != null ? intent.getDataString() : null;
    }
}
