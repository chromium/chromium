// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import android.webkit.TracingConfig;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.android_webview.AwTracingController;
import org.chromium.android_webview.WebViewChromiumRunQueue;
import org.chromium.base.ThreadUtils;
import org.chromium.base.TraceRecordMode;

import java.io.IOException;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.Collection;
import java.util.concurrent.Executor;

/**
 * This class adapts between the common threading and parameter expectations of the
 * webkit framework and support library APIs and those of AwTracingController and
 * makes sure the calls happen on the UI thread.
 * Translates predefined categories and posts callbacks.
 *
 * This class specifically does not rely on android.webkit.TracingConfig and uses only
 * its constants which just need to be available at compile time.
 */
public class SharedTracingControllerAdapter {
    private final WebViewChromiumRunQueue mRunQueue;
    private final AwTracingController mAwTracingController;

    public boolean isTracing() {
        if (checkNeedsPost()) {
            return mRunQueue.runOnUiThreadBlocking(mAwTracingController::isTracing);
        }
        return mAwTracingController.isTracing();
    }

    public SharedTracingControllerAdapter(
            WebViewChromiumRunQueue runQueue, AwTracingController controller) {
        mRunQueue = runQueue;
        mAwTracingController = controller;
    }

    private static int convertAndroidTracingMode(int tracingMode) {
        switch (tracingMode) {
            case TracingConfig.RECORD_UNTIL_FULL:
                return TraceRecordMode.RECORD_UNTIL_FULL;
            case TracingConfig.RECORD_CONTINUOUSLY:
                return TraceRecordMode.RECORD_CONTINUOUSLY;
        }
        return TraceRecordMode.RECORD_CONTINUOUSLY;
    }

    private static boolean categoryIsSet(int bitmask, int categoryMask) {
        return (bitmask & categoryMask) == categoryMask;
    }

    private static Collection<Integer> collectPredefinedCategories(int bitmask) {
        ArrayList<Integer> predefinedIndices = new ArrayList<>();
        // CATEGORIES_NONE is skipped on purpose.
        if (categoryIsSet(bitmask, TracingConfig.CATEGORIES_ALL)) {
            predefinedIndices.add(AwTracingController.CATEGORIES_ALL);
        }
        if (categoryIsSet(bitmask, TracingConfig.CATEGORIES_ANDROID_WEBVIEW)) {
            predefinedIndices.add(AwTracingController.CATEGORIES_ANDROID_WEBVIEW);
        }
        if (categoryIsSet(bitmask, TracingConfig.CATEGORIES_WEB_DEVELOPER)) {
            predefinedIndices.add(AwTracingController.CATEGORIES_WEB_DEVELOPER);
        }
        if (categoryIsSet(bitmask, TracingConfig.CATEGORIES_INPUT_LATENCY)) {
            predefinedIndices.add(AwTracingController.CATEGORIES_INPUT_LATENCY);
        }
        if (categoryIsSet(bitmask, TracingConfig.CATEGORIES_RENDERING)) {
            predefinedIndices.add(AwTracingController.CATEGORIES_RENDERING);
        }
        if (categoryIsSet(bitmask, TracingConfig.CATEGORIES_JAVASCRIPT_AND_RENDERING)) {
            predefinedIndices.add(AwTracingController.CATEGORIES_JAVASCRIPT_AND_RENDERING);
        }
        if (categoryIsSet(bitmask, TracingConfig.CATEGORIES_FRAME_VIEWER)) {
            predefinedIndices.add(AwTracingController.CATEGORIES_FRAME_VIEWER);
        }
        return predefinedIndices;
    }

    private int startOnUI(int predefinedCategories, Collection<String> customIncludedCategories,
                          int tracingMode) {
        return mAwTracingController.start(
                collectPredefinedCategories(predefinedCategories),
                customIncludedCategories, convertAndroidTracingMode(tracingMode));
    }

    public void start(int predefinedCategories, Collection<String> customIncludedCategories,
                      int tracingMode) {
        int result = checkNeedsPost() ?
            mRunQueue.runOnUiThreadBlocking(
                    () -> startOnUI(predefinedCategories, customIncludedCategories, tracingMode)) :
            startOnUI(predefinedCategories, customIncludedCategories, tracingMode);

        if (result != AwTracingController.RESULT_SUCCESS) {
            // make sure to throw on the original calling thread.
            switch (result) {
                case AwTracingController.RESULT_ALREADY_TRACING:
                    throw new IllegalStateException(
                            "cannot start tracing: tracing is already enabled");
                case AwTracingController.RESULT_INVALID_CATEGORIES:
                    throw new IllegalArgumentException(
                            "category patterns starting with '-' or containing ','"
                                    + " are not allowed");
                case AwTracingController.RESULT_INVALID_MODE:
                    throw new IllegalArgumentException("invalid tracing mode");
            }
        }
    }

    public boolean stop(@Nullable OutputStream outputStream, @NonNull Executor executor) {
        return checkNeedsPost() ?
            mRunQueue.runOnUiThreadBlocking(
                    () -> stopOnUI(outputStream, executor)):
            stopOnUI(outputStream, executor);
    }

    private boolean stopOnUI(@Nullable OutputStream outputStream, @NonNull Executor executor) {
        if (outputStream == null) {
            return mAwTracingController.stopAndFlush(null);
        }

        final OutputStream localOutputStream = outputStream;
        return mAwTracingController.stopAndFlush(new OutputStream() {
            @Override
            public void write(byte[] chunk) {
                executor.execute(() -> {
                    try {
                        localOutputStream.write(chunk);
                    } catch (IOException e) {
                        throw new RuntimeException(e);
                    }
                });
            }
            @Override
            public void close() {
                executor.execute(() -> {
                    try {
                        localOutputStream.close();
                    } catch (IOException e) {
                        throw new RuntimeException(e);
                    }
                });
            }
            @Override
            public void write(int b) { /* should not be called */
            }
            @Override
            public void flush() { /* should not be called */
            }
            @Override
            public void write(byte[] b, int off, int len) { /* should not be called */
            }
        });
    }

    private static boolean checkNeedsPost() {
        return !ThreadUtils.runningOnUiThread();
    }
}
