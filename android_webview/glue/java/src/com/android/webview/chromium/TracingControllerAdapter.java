// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import android.annotation.SuppressLint;
import android.webkit.TracingConfig;
import android.webkit.TracingController;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import java.io.OutputStream;
import java.util.concurrent.Executor;

/**
 * Chromium implementation of TracingController -- forwards calls to
 * the shared internal implementation.
 */
@SuppressLint({"NewApi", // TracingController is new in API level 28.
        "Override"}) // Remove this once lint is targeting API level 28.
public class TracingControllerAdapter extends TracingController {
    private final SharedTracingControllerAdapter mTracingController;

    public TracingControllerAdapter(SharedTracingControllerAdapter tracingController) {
        mTracingController = tracingController;
    }

    @Override
    public void start(@NonNull TracingConfig tracingConfig) {
        if (tracingConfig == null) {
            throw new IllegalArgumentException("tracingConfig cannot be null");
        }

        mTracingController.start(tracingConfig.getPredefinedCategories(),
                tracingConfig.getCustomIncludedCategories(), tracingConfig.getTracingMode());
    }

    @Override
    public boolean stop(@Nullable OutputStream outputStream, @NonNull Executor executor) {
        return mTracingController.stop(outputStream, executor);
    }

    @Override
    public boolean isTracing() {
        return mTracingController.isTracing();
    }
}
