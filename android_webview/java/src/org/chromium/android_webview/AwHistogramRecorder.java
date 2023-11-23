// Copyright 2019 The Chromium Authors
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
    @IntDef({
        WebViewCallbackType.ON_RECEIVED_LOGIN_REQUEST,
        WebViewCallbackType.ON_RECEIVED_CLIENT_CERT_REQUEST,
        WebViewCallbackType.ON_RECEIVED_HTTP_AUTH_REQUEST,
        WebViewCallbackType.ON_DOWNLOAD_START,
        WebViewCallbackType.ON_PAGE_STARTED,
        WebViewCallbackType.ON_PAGE_FINISHED,
        WebViewCallbackType.ON_LOAD_RESOURCE,
        WebViewCallbackType.ON_PAGE_COMMIT_VISIBLE,
        WebViewCallbackType.SHOULD_OVERRIDE_URL_LOADING,
        WebViewCallbackType.SHOULD_INTERCEPT_REQUEST,
        WebViewCallbackType.ON_RECEIVED_ERROR,
        WebViewCallbackType.ON_SAFE_BROWSING_HIT,
        WebViewCallbackType.ON_RECEIVED_HTTP_ERROR,
        WebViewCallbackType.GET_VISITED_HISTORY,
        WebViewCallbackType.DO_UPDATE_VISITED_HISTORY,
        WebViewCallbackType.ON_PROGRESS_CHANGED,
        WebViewCallbackType.ON_UNHANDLED_KEY_EVENT,
        WebViewCallbackType.ON_CONSOLE_MESSAGE,
        WebViewCallbackType.ON_CREATE_WINDOW,
        WebViewCallbackType.ON_CLOSE_WINDOW,
        WebViewCallbackType.ON_REQUEST_FOCUS,
        WebViewCallbackType.ON_RECEIVED_TOUCH_ICON_URL,
        WebViewCallbackType.ON_RECEIVED_ICON,
        WebViewCallbackType.ON_RECEIVED_TITLE,
        WebViewCallbackType.SHOULD_OVERRIDE_KEY_EVENT,
        WebViewCallbackType.ON_GEOLOCATION_PERMISSIONS_SHOW_PROMPT,
        WebViewCallbackType.ON_GEOLOCATION_PERMISSIONS_HIDE_PROMPT,
        WebViewCallbackType.ON_PERMISSION_REQUEST,
        WebViewCallbackType.ON_PERMISSION_REQUEST_CANCELED,
        WebViewCallbackType.ON_JS_ALERT,
        WebViewCallbackType.ON_JS_BEFORE_UNLOAD,
        WebViewCallbackType.ON_JS_CONFIRM,
        WebViewCallbackType.ON_JS_PROMPT,
        WebViewCallbackType.ON_RECEIVED_SSL_ERROR,
        WebViewCallbackType.ON_FORM_RESUBMISSION,
        WebViewCallbackType.ON_SHOW_CUSTOM_VIEW,
        WebViewCallbackType.ON_HIDE_CUSTOM_VIEW,
        WebViewCallbackType.GET_DEFAULT_VIDEO_POSTER,
        WebViewCallbackType.ON_RENDER_PROCESS_GONE,
        WebViewCallbackType.ON_SCALE_CHANGED
    })
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
        int SHOULD_INTERCEPT_REQUEST = 9;
        int ON_RECEIVED_ERROR = 10;
        int ON_SAFE_BROWSING_HIT = 11;
        int ON_RECEIVED_HTTP_ERROR = 12;
        int GET_VISITED_HISTORY = 13;
        int DO_UPDATE_VISITED_HISTORY = 14;
        int ON_PROGRESS_CHANGED = 15;
        int ON_UNHANDLED_KEY_EVENT = 16;
        int ON_CONSOLE_MESSAGE = 17;
        int ON_CREATE_WINDOW = 18;
        int ON_CLOSE_WINDOW = 19;
        int ON_REQUEST_FOCUS = 20;
        int ON_RECEIVED_TOUCH_ICON_URL = 21;
        int ON_RECEIVED_ICON = 22;
        int ON_RECEIVED_TITLE = 23;
        int SHOULD_OVERRIDE_KEY_EVENT = 24;
        int ON_GEOLOCATION_PERMISSIONS_SHOW_PROMPT = 25;
        int ON_GEOLOCATION_PERMISSIONS_HIDE_PROMPT = 26;
        int ON_PERMISSION_REQUEST = 27;
        int ON_PERMISSION_REQUEST_CANCELED = 28;
        int ON_JS_ALERT = 29;
        int ON_JS_BEFORE_UNLOAD = 30;
        int ON_JS_CONFIRM = 31;
        int ON_JS_PROMPT = 32;
        int ON_RECEIVED_SSL_ERROR = 33;
        int ON_FORM_RESUBMISSION = 34;
        int ON_SHOW_CUSTOM_VIEW = 35;
        int ON_HIDE_CUSTOM_VIEW = 36;
        int GET_DEFAULT_VIDEO_POSTER = 37;
        int ON_RENDER_PROCESS_GONE = 38;
        int ON_SCALE_CHANGED = 39;
        int NUM_ENTRIES = 40;
    }

    public static void recordCallbackInvocation(@WebViewCallbackType int result) {
        RecordHistogram.recordEnumeratedHistogram(
                "Android.WebView.Callback.Counts", result, WebViewCallbackType.NUM_ENTRIES);
    }

    // not meant to be instantiated
    private AwHistogramRecorder() {}
}
