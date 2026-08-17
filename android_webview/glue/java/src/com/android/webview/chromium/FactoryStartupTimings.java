// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import android.os.Build;
import android.os.Looper;
import android.os.SystemClock;
import android.webkit.WebViewDelegate;
import android.webkit.WebViewFactory;

import org.chromium.base.TraceEvent;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;

/**
 * Immutable value class holding Stage 1 factory provider initialization timings and handling
 * histogram and trace recording for early framework/provider startup.
 */
@NullMarked
class FactoryStartupTimings {
    private static final String HISTOGRAM_STAGE1_FACTORY_INIT =
            "Android.WebView.Startup.CreationTime.Stage1.FactoryInit";
    private static final String HISTOGRAM_TOTAL_FACTORY_INIT_TIME =
            "Android.WebView.Startup.CreationTime.TotalFactoryInitTime";
    private static final String HISTOGRAM_TOTAL_FACTORY_INIT_TIME_MAIN_LOOPER =
            "Android.WebView.Startup.CreationTime.TotalFactoryInitTime.MainLooper";
    private static final String HISTOGRAM_TOTAL_FACTORY_INIT_TIME_NOT_MAIN_LOOPER =
            "Android.WebView.Startup.CreationTime.TotalFactoryInitTime.NotMainLooper";
    private static final String HISTOGRAM_CREATE_CONTEXT_TIME =
            "Android.WebView.Startup.CreationTime.CreateContextTime";
    private static final String HISTOGRAM_ASSETS_ADD_TIME =
            "Android.WebView.Startup.CreationTime.AssetsAddTime";
    private static final String HISTOGRAM_GET_CLASS_LOADER_TIME =
            "Android.WebView.Startup.CreationTime.GetClassLoaderTime";
    private static final String HISTOGRAM_NATIVE_LOAD_TIME =
            "Android.WebView.Startup.CreationTime.NativeLoadTime";
    private static final String HISTOGRAM_GET_PROVIDER_CLASS_TIME =
            "Android.WebView.Startup.CreationTime.GetProviderClassForNameTime";

    final long mStartTime;
    final long mDuration;
    final long mTotalFactoryInitStartTime;
    final long mTotalFactoryInitDuration;

    FactoryStartupTimings(long startTime, WebViewDelegate webViewDelegate) {
        mStartTime = startTime;
        mDuration = SystemClock.uptimeMillis() - startTime;
        RecordHistogram.recordTimesHistogram(HISTOGRAM_STAGE1_FACTORY_INIT, mDuration);

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            WebViewFactory.StartupTimestamps startupTimestamps =
                    webViewDelegate.getStartupTimestamps();
            mTotalFactoryInitStartTime = startupTimestamps.getWebViewLoadStart();
            mTotalFactoryInitDuration = SystemClock.uptimeMillis() - mTotalFactoryInitStartTime;

            boolean isMainLooper = Looper.myLooper() == Looper.getMainLooper();
            RecordHistogram.recordTimesHistogram(
                    HISTOGRAM_TOTAL_FACTORY_INIT_TIME, mTotalFactoryInitDuration);
            if (isMainLooper) {
                RecordHistogram.recordTimesHistogram(
                        HISTOGRAM_TOTAL_FACTORY_INIT_TIME_MAIN_LOOPER, mTotalFactoryInitDuration);
            } else {
                RecordHistogram.recordTimesHistogram(
                        HISTOGRAM_TOTAL_FACTORY_INIT_TIME_NOT_MAIN_LOOPER,
                        mTotalFactoryInitDuration);
            }

            RecordHistogram.recordTimesHistogram(
                    HISTOGRAM_CREATE_CONTEXT_TIME,
                    startupTimestamps.getCreateContextEnd()
                            - startupTimestamps.getCreateContextStart());
            RecordHistogram.recordTimesHistogram(
                    HISTOGRAM_ASSETS_ADD_TIME,
                    startupTimestamps.getAddAssetsEnd() - startupTimestamps.getAddAssetsStart());
            RecordHistogram.recordTimesHistogram(
                    HISTOGRAM_GET_CLASS_LOADER_TIME,
                    startupTimestamps.getGetClassLoaderEnd()
                            - startupTimestamps.getGetClassLoaderStart());
            RecordHistogram.recordTimesHistogram(
                    HISTOGRAM_NATIVE_LOAD_TIME,
                    startupTimestamps.getNativeLoadEnd() - startupTimestamps.getNativeLoadStart());
            RecordHistogram.recordTimesHistogram(
                    HISTOGRAM_GET_PROVIDER_CLASS_TIME,
                    startupTimestamps.getProviderClassForNameEnd()
                            - startupTimestamps.getProviderClassForNameStart());
        } else {
            mTotalFactoryInitStartTime = 0;
            mTotalFactoryInitDuration = 0;
        }
    }

    /** Emits trace events for the earlier Stage 1 factory initialization once tracing is ready. */
    void recordInitTraces() {
        if (mTotalFactoryInitStartTime != 0) {
            TraceEvent.webViewStartupTotalFactoryInit(
                    mTotalFactoryInitStartTime, mTotalFactoryInitDuration);
        }
        TraceEvent.webViewStartupStage1(mStartTime, mDuration);
    }
}
