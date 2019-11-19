// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.text.TextUtils;

import androidx.annotation.Nullable;

import org.chromium.base.TraceRecordMode;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

import java.io.IOException;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.HashSet;
import java.util.List;

/**
 * Manages tracing functionality in WebView.
 */
@JNINamespace("android_webview")
public class AwTracingController {
    private static final String TAG = "AwTracingController";

    public static final int RESULT_SUCCESS = 0;
    public static final int RESULT_ALREADY_TRACING = 1;
    public static final int RESULT_INVALID_CATEGORIES = 2;
    public static final int RESULT_INVALID_MODE = 3;

    public static final int CATEGORIES_ALL = 0;
    public static final int CATEGORIES_ANDROID_WEBVIEW = 1;
    public static final int CATEGORIES_WEB_DEVELOPER = 2;
    public static final int CATEGORIES_INPUT_LATENCY = 3;
    public static final int CATEGORIES_RENDERING = 4;
    public static final int CATEGORIES_JAVASCRIPT_AND_RENDERING = 5;
    public static final int CATEGORIES_FRAME_VIEWER = 6;

    private static final List<String> CATEGORIES_ALL_LIST = new ArrayList<>(Arrays.asList("*"));
    private static final List<String> CATEGORIES_ANDROID_WEBVIEW_LIST =
            new ArrayList<>(Arrays.asList("android_webview", "Java", "toplevel"));
    private static final List<String> CATEGORIES_WEB_DEVELOPER_LIST = new ArrayList<>(
            Arrays.asList("blink", "cc", "netlog", "renderer.scheduler", "toplevel", "v8"));
    private static final List<String> CATEGORIES_INPUT_LATENCY_LIST = new ArrayList<>(
            Arrays.asList("benchmark", "input", "evdev", "renderer.scheduler", "toplevel"));
    private static final List<String> CATEGORIES_RENDERING_LIST =
            new ArrayList<>(Arrays.asList("blink", "cc", "gpu", "toplevel"));
    private static final List<String> CATEGORIES_JAVASCRIPT_AND_RENDERING_LIST = new ArrayList<>(
            Arrays.asList("blink", "cc", "gpu", "renderer.scheduler", "v8", "toplevel"));
    private static final List<String> CATEGORIES_FRAME_VIEWER_LIST = new ArrayList<>(
            Arrays.asList("blink", "cc", "gpu", "renderer.scheduler", "v8", "toplevel",
                    "disabled-by-default-cc.debug", "disabled-by-default-cc.debug.picture",
                    "disabled-by-default-cc.debug.display_items"));

    private static final List<List<String>> PREDEFINED_CATEGORIES_LIST =
            new ArrayList<List<String>>(Arrays.asList(CATEGORIES_ALL_LIST, // CATEGORIES_ALL
                    CATEGORIES_ANDROID_WEBVIEW_LIST, // CATEGORIES_ANDROID_WEBVIEW
                    CATEGORIES_WEB_DEVELOPER_LIST, // CATEGORIES_WEB_DEVELOPER
                    CATEGORIES_INPUT_LATENCY_LIST, // CATEGORIES_INPUT_LATENCY
                    CATEGORIES_RENDERING_LIST, // CATEGORIES_RENDERING
                    CATEGORIES_JAVASCRIPT_AND_RENDERING_LIST, // CATEGORIES_JAVASCRIPT_AND_RENDERING
                    CATEGORIES_FRAME_VIEWER_LIST // CATEGORIES_FRAME_VIEWER
                    ));

    private OutputStream mOutputStream;

    // TODO(timvolodine): consider caching mIsTracing value for efficiency.
    // boolean mIsTracing;

    public AwTracingController() {
        mNativeAwTracingController = AwTracingControllerJni.get().init(AwTracingController.this);
    }

    // Start tracing
    public int start(Collection<Integer> predefinedCategories,
            Collection<String> customIncludedCategories, int mode) {
        if (isTracing()) return RESULT_ALREADY_TRACING;
        if (!isValid(customIncludedCategories)) return RESULT_INVALID_CATEGORIES;
        if (!isValidMode(mode)) return RESULT_INVALID_MODE;

        String categoryFilter =
                constructCategoryFilterString(predefinedCategories, customIncludedCategories);
        AwTracingControllerJni.get().start(
                mNativeAwTracingController, AwTracingController.this, categoryFilter, mode);
        return RESULT_SUCCESS;
    }

    // Stop tracing and flush tracing data.
    public boolean stopAndFlush(@Nullable OutputStream outputStream) {
        if (!isTracing()) return false;
        mOutputStream = outputStream;
        AwTracingControllerJni.get().stopAndFlush(
                mNativeAwTracingController, AwTracingController.this);
        return true;
    }

    public boolean isTracing() {
        return AwTracingControllerJni.get().isTracing(
                mNativeAwTracingController, AwTracingController.this);
    }

    // Combines configuration bits into a category string usable by chromium.
    // Assumes that customIncludedCategories have been validated using isValid() method.
    private String constructCategoryFilterString(
            Collection<Integer> predefinedCategories, Collection<String> customIncludedCategories) {
        // Make sure to remove any doubles in category patterns.
        HashSet<String> categoriesSet = new HashSet<String>();
        for (int predefinedCategoriesIndex : predefinedCategories) {
            categoriesSet.addAll(PREDEFINED_CATEGORIES_LIST.get(predefinedCategoriesIndex));
        }
        categoriesSet.addAll(customIncludedCategories);
        if (categoriesSet.isEmpty()) {
            // when no categories are specified -- exclude everything
            categoriesSet.add("-*");
        }
        return TextUtils.join(",", categoriesSet);
    }

    // Returns true if the given collection of categories is valid.
    private static boolean isValid(Collection<String> customIncludedCategories) {
        for (String categoryPattern : customIncludedCategories) {
            if (!isValidPattern(categoryPattern)) {
                return false;
            }
        }
        return true;
    }

    private static boolean isValidPattern(String pattern) {
        // do not allow 'excluded' patterns or comma separated strings
        return !pattern.startsWith("-") && !pattern.contains(",");
    }

    private static boolean isValidMode(int mode) {
        // allow only two modes, to ensure limited memory usage.
        return (mode == TraceRecordMode.RECORD_UNTIL_FULL
                || mode == TraceRecordMode.RECORD_CONTINUOUSLY);
    }

    @CalledByNative
    private void onTraceDataChunkReceived(byte[] data) throws IOException {
        if (mOutputStream != null) {
            mOutputStream.write(data);
        }
    }

    @CalledByNative
    private void onTraceDataComplete() throws IOException {
        if (mOutputStream != null) {
            mOutputStream.close();
        }
    }

    private long mNativeAwTracingController;

    @NativeMethods
    interface Natives {
        long init(AwTracingController caller);
        boolean start(long nativeAwTracingController, AwTracingController caller, String categories,
                int traceMode);
        boolean stopAndFlush(long nativeAwTracingController, AwTracingController caller);
        boolean isTracing(long nativeAwTracingController, AwTracingController caller);
    }
}
