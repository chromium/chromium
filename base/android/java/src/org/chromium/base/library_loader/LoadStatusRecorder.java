// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.library_loader;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.metrics.CachedMetrics.EnumeratedHistogramSample;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;

/**
 * Buffers and records UMA histogram(s) about library loading attempts.
 *
 * This class is not thread safe. This class relies on CachedMetrics, hence only supports process
 * types that CacheMetrics support.
 */
public class LoadStatusRecorder {
    // Used to record an UMA histogram. Since these value are persisted to logs, they should never
    // be renumbered nor reused.
    @IntDef({LoadLibraryStatus.IS_BROWSER, LoadLibraryStatus.AT_FIXED_ADDRESS,
            LoadLibraryStatus.FIRST_ATTEMPT, LoadLibraryStatus.WAS_SUCCESSFUL,
            LoadLibraryStatus.BOUNDARY})
    @Retention(RetentionPolicy.SOURCE)
    @VisibleForTesting
    public @interface LoadLibraryStatus {
        int WAS_SUCCESSFUL = 1;
        int AT_FIXED_ADDRESS = 1 << 1;
        int FIRST_ATTEMPT = 1 << 2;
        int IS_BROWSER = 1 << 3;
        int BOUNDARY = 1 << 4;
    }

    private EnumeratedHistogramSample mHistogramSample = new EnumeratedHistogramSample(
            "ChromiumAndroidLinker.LoadLibraryStatus", LoadLibraryStatus.BOUNDARY);

    private @LibraryProcessType int mProcessType = LibraryProcessType.PROCESS_UNINITIALIZED;

    private ArrayList<Integer> mBufferedAttempts = new ArrayList<>(1);

    /**
     * Emits information about library loading attempt as UMA histogram or buffers it until process
     * type is set via {@code setProcessType()}.
     * @param success Whether the attempt to load the library succeeded.
     * @param isFirstAttempt Whether it was the first attempt (not a retry).
     * @param loadAtFixedAddress Whether loading at fixed address was attempted.
     */
    public void recordLoadAttempt(
            boolean success, boolean isFirstAttempt, boolean loadAtFixedAddress) {
        int sample = convertAttemptToInt(success, isFirstAttempt, loadAtFixedAddress);
        if (processTypeWasSet()) {
            recordWithProcessType(sample);
            return;
        }
        mBufferedAttempts.add(sample);
    }

    /** Sets the process type and flushes the buffered recorded attempts to UMA. */
    public void setProcessType(@LibraryProcessType int processType) {
        assert processType != LibraryProcessType.PROCESS_UNINITIALIZED;
        assert !processTypeWasSet() || mProcessType == processType;
        mProcessType = processType;
        for (int sample : mBufferedAttempts) recordWithProcessType(sample);
        mBufferedAttempts.clear();
    }

    private boolean processTypeWasSet() {
        return mProcessType != LibraryProcessType.PROCESS_UNINITIALIZED;
    }

    private static int convertAttemptToInt(
            boolean success, boolean isFirstAttempt, boolean loadAtFixedAddress) {
        int sample = 0;
        if (success) sample |= LoadLibraryStatus.WAS_SUCCESSFUL;
        if (loadAtFixedAddress) sample |= LoadLibraryStatus.AT_FIXED_ADDRESS;
        if (isFirstAttempt) sample |= LoadLibraryStatus.FIRST_ATTEMPT;
        return sample;
    }

    private void recordWithProcessType(int sample) {
        if (mProcessType == LibraryProcessType.PROCESS_BROWSER
                || mProcessType == LibraryProcessType.PROCESS_WEBVIEW
                || mProcessType == LibraryProcessType.PROCESS_WEBLAYER) {
            sample |= LoadLibraryStatus.IS_BROWSER;
        }
        mHistogramSample.record(sample);
    }
}
