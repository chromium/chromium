// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_glue;

import static org.chromium.support_lib_glue.SupportLibWebViewChromiumFactory.recordApiCall;

import com.android.webview.chromium.SharedTracingControllerAdapter;

import org.chromium.base.TraceEvent;
import org.chromium.support_lib_boundary.TracingControllerBoundaryInterface;
import org.chromium.support_lib_glue.SupportLibWebViewChromiumFactory.ApiCall;

import java.io.OutputStream;
import java.util.Collection;
import java.util.concurrent.Executor;

/** Adapter between AwTracingController and TracingControllerBoundaryInterface. */
public class SupportLibTracingControllerAdapter implements TracingControllerBoundaryInterface {
    private final SharedTracingControllerAdapter mTracingController;

    public SupportLibTracingControllerAdapter(SharedTracingControllerAdapter tracingController) {
        mTracingController = tracingController;
    }

    @Override
    public boolean isTracing() {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.AndroidX.TRACING_CONTROLLER_IS_TRACING")) {
            recordApiCall(ApiCall.TRACING_CONTROLLER_IS_TRACING);
            return mTracingController.isTracing();
        }
    }

    @Override
    public void start(
            int predefinedCategories, Collection<String> customIncludedCategories, int mode)
            throws IllegalStateException, IllegalArgumentException {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.AndroidX.TRACING_CONTROLLER_START")) {
            recordApiCall(ApiCall.TRACING_CONTROLLER_START);
            mTracingController.start(predefinedCategories, customIncludedCategories, mode);
        }
    }

    @Override
    public boolean stop(OutputStream outputStream, Executor executor) {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.AndroidX.TRACING_CONTROLLER_STOP")) {
            recordApiCall(ApiCall.TRACING_CONTROLLER_STOP);
            return mTracingController.stop(outputStream, executor);
        }
    }
}
