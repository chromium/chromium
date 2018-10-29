// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_glue;

import com.android.webview.chromium.SharedTracingControllerAdapter;

import org.chromium.support_lib_boundary.TracingControllerBoundaryInterface;

import java.io.OutputStream;
import java.util.Collection;
import java.util.concurrent.Executor;

/**
 * Adapter between AwTracingController and TracingControllerBoundaryInterface.
 */
public class SupportLibTracingControllerAdapter implements TracingControllerBoundaryInterface {
    private final SharedTracingControllerAdapter mTracingController;

    public SupportLibTracingControllerAdapter(SharedTracingControllerAdapter tracingController) {
        mTracingController = tracingController;
    }

    @Override
    public boolean isTracing() {
        return mTracingController.isTracing();
    }

    @Override
    public void start(int predefinedCategories,
                      Collection<String> customIncludedCategories, int mode)
            throws IllegalStateException, IllegalArgumentException {
        mTracingController.start(predefinedCategories, customIncludedCategories, mode);
    }

    @Override
    public boolean stop(OutputStream outputStream, Executor executor) {
        return mTracingController.stop(outputStream, executor);
    }
}
