// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.metrics;

import android.os.SystemClock;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.MainDex;
import org.chromium.base.annotations.NativeMethods;

import java.util.Collections;
import java.util.HashMap;
import java.util.Map;

/**
 * An implementation of {@link UmaRecorder} which forwards all calls through JNI.
 *
 * Note: the JNI calls are relatively costly - avoid calling these methods in performance-critical
 * code.
 */
@JNINamespace("base::android")
@MainDex
/* package */ final class NativeUmaRecorder implements UmaRecorder {
    /**
     * Internally, histograms objects are cached on the Java side by their pointer
     * values (converted to long). This is safe to do because C++ Histogram objects
     * are never freed. Caching them on the Java side prevents needing to do costly
     * Java String to C++ string conversions on the C++ side during lookup.
     */
    private final Map<String, Long> mNativeHints =
            Collections.synchronizedMap(new HashMap<String, Long>());

    @Override
    public void recordBooleanHistogram(String name, boolean sample) {
        long oldHint = getNativeHint(name);
        long newHint = NativeUmaRecorderJni.get().recordBooleanHistogram(name, oldHint, sample);
        maybeUpdateNativeHint(name, oldHint, newHint);
    }

    @Override
    public void recordExponentialHistogram(
            String name, int sample, int min, int max, int numBuckets) {
        long oldHint = getNativeHint(name);
        long newHint = NativeUmaRecorderJni.get().recordExponentialHistogram(
                name, oldHint, sample, min, max, numBuckets);
        maybeUpdateNativeHint(name, oldHint, newHint);
    }

    @Override
    public void recordLinearHistogram(String name, int sample, int min, int max, int numBuckets) {
        long oldHint = getNativeHint(name);
        long newHint = NativeUmaRecorderJni.get().recordLinearHistogram(
                name, oldHint, sample, min, max, numBuckets);
        maybeUpdateNativeHint(name, oldHint, newHint);
    }

    @Override
    public void recordSparseHistogram(String name, int sample) {
        long oldHint = getNativeHint(name);
        long newHint = NativeUmaRecorderJni.get().recordSparseHistogram(name, oldHint, sample);
        maybeUpdateNativeHint(name, oldHint, newHint);
    }

    @Override
    public void recordUserAction(String name, long elapsedRealtimeMillis) {
        // Java and native code use different clocks. We need a relative elapsed time.
        long millisSinceEvent = SystemClock.elapsedRealtime() - elapsedRealtimeMillis;
        NativeUmaRecorderJni.get().recordUserAction(name, millisSinceEvent);
    }

    private long getNativeHint(String name) {
        Long hint = mNativeHints.get(name);
        // Note: If key is null, we don't have it cached. In that case, pass 0
        // to the native code, which gets converted to a null histogram pointer
        // which will cause the native code to look up the object on the native
        // side.
        return (hint == null ? 0 : hint);
    }

    private void maybeUpdateNativeHint(String name, long oldHint, long newHint) {
        if (oldHint != newHint) {
            mNativeHints.put(name, newHint);
        }
    }

    /** Natives API to record metrics. */
    @NativeMethods
    interface Natives {
        long recordBooleanHistogram(String name, long nativeHint, boolean sample);
        long recordExponentialHistogram(
                String name, long nativeHint, int sample, int min, int max, int numBuckets);
        long recordLinearHistogram(
                String name, long nativeHint, int sample, int min, int max, int numBuckets);
        long recordSparseHistogram(String name, long nativeHint, int sample);

        /**
         * Records that the user performed an action. See {@code base::RecordComputedActionAt}.
         * <p>
         * Uses relative time, because Java and native code can use different clocks.
         *
         * @param name Name of the user-generated event.
         * @param millisSinceEvent difference between now and the time when the event was observed.
         *         Should be positive.
         */
        void recordUserAction(String name, long millisSinceEvent);
    }
}
