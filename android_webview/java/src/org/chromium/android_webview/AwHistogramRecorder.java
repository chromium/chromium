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

    @Retention(RetentionPolicy.SOURCE)
    @IntDef({MimeType.NULL_FROM_CONTENT_PROVIDER, MimeType.NONNULL_FROM_CONTENT_PROVIDER,
            MimeType.CANNOT_GUESS_FROM_ANDROID_ASSET_PATH, MimeType.GUESSED_FROM_ANDROID_ASSET_PATH,
            MimeType.CANNOT_GUESS_FROM_ANDROID_ASSET_INPUT_STREAM,
            MimeType.GUESSED_FROM_ANDROID_ASSET_INPUT_STREAM})
    public @interface MimeType {
        // These values are persisted to logs. Entries should not be renumbered and numeric values
        // should never be reused. Update NUM_ENTRIES when adding more entries.
        int NULL_FROM_CONTENT_PROVIDER = 0;
        int NONNULL_FROM_CONTENT_PROVIDER = 1;
        int CANNOT_GUESS_FROM_ANDROID_ASSET_PATH = 2;
        int GUESSED_FROM_ANDROID_ASSET_PATH = 3;
        int CANNOT_GUESS_DUE_TO_GENERIC_EXCEPTION = 4;
        int CANNOT_GUESS_FROM_ANDROID_ASSET_INPUT_STREAM = 5;
        int GUESSED_FROM_ANDROID_ASSET_INPUT_STREAM = 6;
        int CANNOT_GUESS_DUE_TO_IO_EXCEPTION = 7;
        int NULL_FROM_SHOULD_INTERCEPT_REQUEST = 8;
        int NONNULL_FROM_SHOULD_INTERCEPT_REQUEST = 9;
        int NUM_ENTRIES = 10;
    }

    public static void recordMimeType(@MimeType int type) {
        RecordHistogram.recordEnumeratedHistogram(
                "Android.WebView.Mimetype.AppProvided", type, MimeType.NUM_ENTRIES);
    }

    // not meant to be instantiated
    private AwHistogramRecorder() {}
}
