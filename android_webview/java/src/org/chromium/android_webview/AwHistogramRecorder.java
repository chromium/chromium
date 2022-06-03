// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Collect information about Android WebView usage. Adding metrics to this class can be helpful if
 * you need to log the same metric from different call sites in different Java classes.
 *
 * <p>If you only need to log at a single call site, prefer calling {@link RecordHistogram} methods
 * directly.
 */
public class AwHistogramRecorder {
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({WebViewCallbackType.ON_RECEIVED_LOGIN_REQUEST,
            WebViewCallbackType.ON_RECEIVED_CLIENT_CERT_REQUEST,
            WebViewCallbackType.ON_RECEIVED_HTTP_AUTH_REQUEST,
            WebViewCallbackType.ON_DOWNLOAD_START, WebViewCallbackType.ON_PAGE_STARTED,
            WebViewCallbackType.ON_PAGE_FINISHED, WebViewCallbackType.ON_LOAD_RESOURCE,
            WebViewCallbackType.ON_PAGE_COMMIT_VISIBLE,
            WebViewCallbackType.SHOULD_OVERRIDE_URL_LOADING})
    public @interface WebViewCallbackType {
        // These values are used for UMA. Don't reuse or reorder values.
        // If you add something, update NUM_ENTRIES.
        int ON_RECEIVED_LOGIN_REQUEST = 0;
        int ON_RECEIVED_CLIENT_CERT_REQUEST = 1;
        int ON_RECEIVED_HTTP_AUTH_REQUEST = 2;
        int ON_DOWNLOAD_START = 3;
        int ON_PAGE_STARTED = 4;
        int ON_PAGE_FINISHED = 5;
        int ON_LOAD_RESOURCE = 6;
        int ON_PAGE_COMMIT_VISIBLE = 7;
        int SHOULD_OVERRIDE_URL_LOADING = 8;
        int NUM_ENTRIES = 9;
    }

    public static void recordCallbackInvocation(@WebViewCallbackType int result) {
        RecordHistogram.recordEnumeratedHistogram(
                "Android.WebView.Callback.Counts", result, WebViewCallbackType.NUM_ENTRIES);
    }

    // not meant to be instantiated
    private AwHistogramRecorder() {}
}
