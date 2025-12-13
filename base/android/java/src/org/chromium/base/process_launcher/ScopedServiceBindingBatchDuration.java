// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.process_launcher;

import org.chromium.base.TimeUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Records the duration of the batching scope on the main thread. */
@NullMarked
/*package*/ final class ScopedServiceBindingBatchDuration implements ScopedServiceBindingBatch {
    private static @Nullable ScopedServiceBindingBatchDuration sInstance;
    private final long mStartTimeMillis;

    /**
     * Try to start measuring the duration of the batching scope.
     *
     * <p>This returns null if there is already a batching scope being measured. This is to measure
     * the root most batching scope in nested batching scopes.
     */
    static @Nullable ScopedServiceBindingBatchDuration tryMeasure() {
        if (sInstance != null) {
            return null;
        }

        sInstance = new ScopedServiceBindingBatchDuration();
        return sInstance;
    }

    private ScopedServiceBindingBatchDuration() {
        mStartTimeMillis = TimeUtils.uptimeMillis();
    }

    @Override
    public void close() {
        RecordHistogram.recordTimesHistogram(
                "Android.ChildProcessBinding.BatchScopeDurationOnMainThread",
                TimeUtils.uptimeMillis() - mStartTimeMillis);
        sInstance = null;
    }
}
