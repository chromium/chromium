// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.metrics;

import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.base.TimeUtils;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * An implementation of {@link UmaRecorder} which forwards all calls through JNI.
 *
 * Note: the JNI calls are relatively costly - avoid calling these methods in performance-critical
 * code.
 */
@JNINamespace("base::android")
/* package */ final class NativeUmaRecorder implements UmaRecorder {
    /**
     * Internally, histograms objects are cached on the Java side by their pointer
     * values (converted to long). This is safe to do because C++ Histogram objects
     * are never freed. Caching them on the Java side prevents needing to do costly
     * Java String to C++ string conversions on the C++ side during lookup.
     */
    private final Map<String, Long> mNativeHints =
            Collections.synchronizedMap(new HashMap<String, Long>());

    private Map<Callback<String>, Long> mUserActionTestingCallbackNativePtrs;

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
        long newHint =
                NativeUmaRecorderJni.get()
                        .recordExponentialHistogram(name, oldHint, sample, min, max, numBuckets);
        maybeUpdateNativeHint(name, oldHint, newHint);
    }

    @Override
    public void recordLinearHistogram(String name, int sample, int min, int max, int numBuckets) {
        long oldHint = getNativeHint(name);
        long newHint =
                NativeUmaRecorderJni.get()
                        .recordLinearHistogram(name, oldHint, sample, min, max, numBuckets);
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
        long millisSinceEvent = TimeUtils.elapsedRealtimeMillis() - elapsedRealtimeMillis;
        NativeUmaRecorderJni.get().recordUserAction(name, millisSinceEvent);
    }

    @Override
    public int getHistogramValueCountForTesting(String name, int sample) {
        return NativeUmaRecorderJni.get().getHistogramValueCountForTesting(name, sample, 0);
    }

    @Override
    public int getHistogramTotalCountForTesting(String name) {
        return NativeUmaRecorderJni.get().getHistogramTotalCountForTesting(name, 0);
    }

    @Override
    public List<HistogramBucket> getHistogramSamplesForTesting(String name) {
        long[] samplesArray = NativeUmaRecorderJni.get().getHistogramSamplesForTesting(name);
        List<HistogramBucket> buckets = new ArrayList<>(samplesArray.length);
        for (int i = 0; i < samplesArray.length; i += 3) {
            int min = (int) samplesArray[i];
            long max = samplesArray[i + 1];
            int count = (int) samplesArray[i + 2];
            buckets.add(new HistogramBucket(min, max, count));
        }
        return buckets;
    }

    @Override
    public void addUserActionCallbackForTesting(Callback<String> callback) {
        long ptr = NativeUmaRecorderJni.get().addActionCallbackForTesting(callback);
        if (mUserActionTestingCallbackNativePtrs == null) {
            mUserActionTestingCallbackNativePtrs = Collections.synchronizedMap(new HashMap<>());
        }
        mUserActionTestingCallbackNativePtrs.put(callback, ptr);
    }

    @Override
    public void removeUserActionCallbackForTesting(Callback<String> callback) {
        if (mUserActionTestingCallbackNativePtrs == null) {
            assert false
                    : "Attempting to remove a user action callback without previously registering"
                            + " any.";
            return;
        }
        Long ptr = mUserActionTestingCallbackNativePtrs.remove(callback);
        if (ptr == null) {
            assert false
                    : "Attempting to remove a user action callback that was never previously"
                            + " registered.";
            return;
        }
        NativeUmaRecorderJni.get().removeActionCallbackForTesting(ptr);
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
    public interface Natives {
        long recordBooleanHistogram(String name, long nativeHint, boolean sample);

        long recordExponentialHistogram(
                String name, long nativeHint, int sample, int min, int max, int numBuckets);

        long recordLinearHistogram(
                String name, long nativeHint, int sample, int min, int max, int numBuckets);

        long recordSparseHistogram(String name, long nativeHint, int sample);

        /**
         * Records that the user performed an action. See {@code base::RecordComputedActionAt}.
         *
         * <p>Uses relative time, because Java and native code can use different clocks.
         *
         * @param name Name of the user-generated event.
         * @param millisSinceEvent difference between now and the time when the event was observed.
         *     Should be positive.
         */
        void recordUserAction(@JniType("std::string") String name, long millisSinceEvent);

        int getHistogramValueCountForTesting(
                @JniType("std::string") String name, int sample, long snapshotPtr);

        int getHistogramTotalCountForTesting(@JniType("std::string") String name, long snapshotPtr);

        long[] getHistogramSamplesForTesting(@JniType("std::string") String name);

        long createHistogramSnapshotForTesting();

        void destroyHistogramSnapshotForTesting(long snapshotPtr);

        long addActionCallbackForTesting(Callback<String> callback);

        void removeActionCallbackForTesting(long callbackId);
    }
}
