// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.metrics;

import android.annotation.SuppressLint;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.build.BuildConfig;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.locks.ReentrantReadWriteLock;

import javax.annotation.concurrent.GuardedBy;

/**
 * Stores metrics until given an {@link UmaRecorder} to forward the samples to. After flushing, no
 * longer stores metrics, instead immediately forwards them to the given {@link UmaRecorder}.
 */
/* package */ final class CachingUmaRecorder implements UmaRecorder {
    private static final String TAG = "CachingUmaRecorder";

    /**
     * Maximum number of histograms cached at the same time. It is better to drop some samples
     * rather than have a bug cause the cache to grow without limit.
     * <p>
     * Each sample uses 4 bytes, each histogram uses approx. 12 references (at least 4 bytes each).
     * With {@code MAX_HISTOGRAM_COUNT = 256} and {@code MAX_SAMPLE_COUNT = 256} this limits cache
     * size to 270KiB. Changing either value by one, adds or removes approx. 1KiB.
     */
    private static final int MAX_HISTOGRAM_COUNT = 256;

    /**
     * Maximum number of user actions cached at the same time. It is better to drop some samples
     * rather than have a bug cause the cache to grow without limit.
     */
    @VisibleForTesting static final int MAX_USER_ACTION_COUNT = 256;

    /** Stores the definition and samples of a single cached histogram. */
    @VisibleForTesting
    static class Histogram {
        /**
         * Maximum number of cached samples in a single histogram. it is better to drop some samples
         * rather than have a bug cause the cache to grow without limit
         */
        @VisibleForTesting static final int MAX_SAMPLE_COUNT = 256;

        /** Identifies the type of the histogram. */
        @IntDef({
            Type.BOOLEAN,
            Type.EXPONENTIAL,
            Type.LINEAR,
            Type.SPARSE,
        })
        @Retention(RetentionPolicy.SOURCE)
        @interface Type {
            /** Used by histograms recorded with {@link UmaRecorder#recordBooleanHistogram}. */
            int BOOLEAN = 1;

            /** Used by histograms recorded with {@link UmaRecorder#recordExponentialHistogram}. */
            int EXPONENTIAL = 2;

            /** Used by histograms recorded with {@link UmaRecorder#recordLinearHistogram}. */
            int LINEAR = 3;

            /** Used by histograms recorded with {@link UmaRecorder#recordSparseHistogram}. */
            int SPARSE = 4;
        }

        @Type private final int mType;
        private final String mName;

        private final int mMin;
        private final int mMax;
        private final int mNumBuckets;

        @GuardedBy("this")
        private final List<Integer> mSamples;

        /**
         * Constructs a {@code Histogram} with the specified definition and no samples.
         *
         * @param type histogram type.
         * @param name histogram name.
         * @param min histogram min value. Must be {@code 0} for boolean or sparse histograms.
         * @param max histogram max value. Must be {@code 0} for boolean or sparse histograms.
         * @param numBuckets number of histogram buckets. Must be {@code 0} for boolean or sparse
         *         histograms.
         */
        Histogram(@Type int type, String name, int min, int max, int numBuckets) {
            assert type == Type.EXPONENTIAL
                            || type == Type.LINEAR
                            || (min == 0 && max == 0 && numBuckets == 0)
                    : "Histogram type " + type + " must have no min/max/buckets set";
            mType = type;
            mName = name;
            mMin = min;
            mMax = max;
            mNumBuckets = numBuckets;

            mSamples = new ArrayList<>(/* initialCapacity= */ 1);
        }

        /**
         * Appends a sample to values cached in this histogram. Verifies that histogram definition
         * matches the definition used to create this object: attempts to fail with an assertion,
         * otherwise records failure statistics.
         *
         * @param type histogram type.
         * @param name histogram name.
         * @param sample sample value to cache.
         * @param min histogram min value. Must be {@code 0} for boolean or sparse histograms.
         * @param max histogram max value. Must be {@code 0} for boolean or sparse histograms.
         * @param numBuckets number of histogram buckets. Must be {@code 0} for boolean or sparse
         *         histograms.
         * @return true if the sample was recorded.
         */
        synchronized boolean addSample(
                @Type int type, String name, int sample, int min, int max, int numBuckets) {
            assert mType == type;
            assert mName.equals(name);
            assert mMin == min;
            assert mMax == max;
            assert mNumBuckets == numBuckets;
            if (mSamples.size() >= MAX_SAMPLE_COUNT) {
                // A cache filling up is most likely an indication of a bug.
                assert false : "Histogram exceeded sample cache size limit";
                return false;
            }
            mSamples.add(sample);
            return true;
        }

        /**
         * Writes all histogram samples to {@code recorder}, clears the cache.
         *
         * @param recorder destination {@link UmaRecorder}.
         * @return number of flushed histogram samples.
         */
        synchronized int flushTo(UmaRecorder recorder) {
            switch (mType) {
                case Type.BOOLEAN:
                    for (int i = 0; i < mSamples.size(); i++) {
                        final int sample = mSamples.get(i);
                        recorder.recordBooleanHistogram(mName, sample != 0);
                    }
                    break;
                case Type.EXPONENTIAL:
                    for (int i = 0; i < mSamples.size(); i++) {
                        final int sample = mSamples.get(i);
                        recorder.recordExponentialHistogram(mName, sample, mMin, mMax, mNumBuckets);
                    }
                    break;
                case Type.LINEAR:
                    for (int i = 0; i < mSamples.size(); i++) {
                        final int sample = mSamples.get(i);
                        recorder.recordLinearHistogram(mName, sample, mMin, mMax, mNumBuckets);
                    }
                    break;
                case Type.SPARSE:
                    for (int i = 0; i < mSamples.size(); i++) {
                        final int sample = mSamples.get(i);
                        recorder.recordSparseHistogram(mName, sample);
                    }
                    break;
                default:
                    assert false : "Unknown histogram type " + mType;
            }
            int count = mSamples.size();
            mSamples.clear();
            return count;
        }
    }

    /** Stores a single cached user action. */
    private static class UserAction {
        private final String mName;
        private final long mElapsedRealtimeMillis;

        UserAction(String name, long elapsedRealtimeMillis) {
            mName = name;
            mElapsedRealtimeMillis = elapsedRealtimeMillis;
        }

        /** Writes this user action to a {@link UmaRecorder}. */
        void flushTo(UmaRecorder recorder) {
            recorder.recordUserAction(mName, mElapsedRealtimeMillis);
        }
    }

    /**
     * The lock doesn't need to be fair - in the worst case a writing record*Histogram call will be
     * starved until reading calls reach cache size limits.
     *
     * <p>A read-write lock is used rather than {@code synchronized} blocks to the limit
     * opportunities for stutter on the UI thread when waiting for this shared resource.
     */
    private final ReentrantReadWriteLock mRwLock = new ReentrantReadWriteLock(/* fair= */ false);

    /** Cached histograms keyed by histogram name. */
    @GuardedBy("mRwLock")
    private Map<String, Histogram> mHistogramByName = new HashMap<>();

    /**
     * Number of histogram samples that couldn't be cached, because some limit of cache size been
     * reached.
     * <p>
     * Using {@link AtomicInteger} because the value may need to be updated with a read lock held.
     */
    private AtomicInteger mDroppedHistogramSampleCount = new AtomicInteger();

    /** Cache of user actions. */
    @GuardedBy("mRwLock")
    private List<UserAction> mUserActions = new ArrayList<>();

    /**
     * Number of user actions that couldn't be cached, because the number of user actions in cache
     * has reached its limit.
     */
    @GuardedBy("mRwLock")
    private int mDroppedUserActionCount;

    /**
     * If not {@code null}, all metrics are forwarded to this {@link UmaRecorder}.
     * <p>
     * The read lock must be held while invoking methods on {@code mDelegate}.
     */
    @GuardedBy("mRwLock")
    @Nullable
    private UmaRecorder mDelegate;

    @GuardedBy("mRwLock")
    @Nullable
    private List<Callback<String>> mUserActionCallbacksForTesting;

    /**
     * Sets the current delegate to {@code recorder}. Forwards and clears all cached metrics if
     * {@code recorder} is not {@code null}.
     *
     * @param recorder new delegate.
     * @return the previous delegate.
     */
    public UmaRecorder setDelegate(@Nullable final UmaRecorder recorder) {
        UmaRecorder previous;
        Map<String, Histogram> histogramCache = null;
        int droppedHistogramSampleCount = 0;
        List<UserAction> userActionCache = null;
        int droppedUserActionCount = 0;

        mRwLock.writeLock().lock();
        try {
            previous = mDelegate;
            mDelegate = recorder;
            if (BuildConfig.IS_FOR_TEST) {
                swapUserActionCallbacksForTesting(previous, recorder);
            }
            if (recorder == null) {
                return previous;
            }
            if (!mHistogramByName.isEmpty()) {
                histogramCache = mHistogramByName;
                mHistogramByName = new HashMap<>();
                droppedHistogramSampleCount = mDroppedHistogramSampleCount.getAndSet(0);
            }
            if (!mUserActions.isEmpty()) {
                userActionCache = mUserActions;
                mUserActions = new ArrayList<>();
                droppedUserActionCount = mDroppedUserActionCount;
                mDroppedUserActionCount = 0;
            }
            // Downgrade by acquiring read lock before releasing write lock
            mRwLock.readLock().lock();
        } finally {
            mRwLock.writeLock().unlock();
        }
        // Cache is flushed only after downgrading from a write lock to a read lock.
        try {
            if (histogramCache != null) {
                flushHistogramsAlreadyLocked(histogramCache, droppedHistogramSampleCount);
            }
            if (userActionCache != null) {
                flushUserActionsAlreadyLocked(userActionCache, droppedUserActionCount);
            }
        } finally {
            mRwLock.readLock().unlock();
        }
        return previous;
    }

    /**
     * Writes histogram samples from {@code cache} to the delegate. Assumes that a read lock is held
     * by the current thread.
     *
     * @param cache the cache to be flushed.
     * @param droppedHistogramSampleCount number of histogram samples that were not recorded due to
     *         cache size limits.
     */
    @GuardedBy("mRwLock")
    private void flushHistogramsAlreadyLocked(
            Map<String, Histogram> cache, int droppedHistogramSampleCount) {
        assert mDelegate != null : "Unexpected: cache is flushed, but delegate is null";
        assert mRwLock.getReadHoldCount() > 0;
        int flushedHistogramSampleCount = 0;
        final int flushedHistogramCount = cache.size();
        for (Histogram histogram : cache.values()) {
            flushedHistogramSampleCount += histogram.flushTo(mDelegate);
        }
        Log.i(
                TAG,
                "Flushed %d samples from %d histograms, %d samples were dropped.",
                flushedHistogramSampleCount,
                flushedHistogramCount,
                droppedHistogramSampleCount);
    }

    /**
     * Writes user actions from {@code cache} to the delegate. Assumes that a read lock is held by
     * the current thread.
     *
     * @param cache the cache to be flushed.
     * @param droppedUserActionCount number of user actions that were not recorded in {@code cache}
     *         to stay within {@link MAX_USER_ACTION_COUNT}.
     */
    private void flushUserActionsAlreadyLocked(List<UserAction> cache, int droppedUserActionCount) {
        assert mDelegate != null : "Unexpected: cache is flushed, but delegate is null";
        assert mRwLock.getReadHoldCount() > 0;
        for (UserAction userAction : cache) {
            userAction.flushTo(mDelegate);
        }
        Log.i(
                TAG,
                "Flushed %d user action samples, %d samples were dropped.",
                cache.size(),
                droppedUserActionCount);
    }

    /**
     * Forwards or stores a histogram sample. Stores samples iff there is no delegate {@link
     * UmaRecorder} set.
     *
     * @param type histogram type.
     * @param name histogram name.
     * @param sample sample value.
     * @param min histogram min value.
     * @param max histogram max value.
     * @param numBuckets number of histogram buckets.
     */
    private void cacheOrRecordHistogramSample(
            @Histogram.Type int type, String name, int sample, int min, int max, int numBuckets) {
        // Optimistic attempt without creating a Histogram.
        if (tryAppendOrRecordSample(type, name, sample, min, max, numBuckets)) {
            return;
        }

        mRwLock.writeLock().lock();
        try {
            if (mDelegate == null) {
                cacheHistogramSampleAlreadyWriteLocked(type, name, sample, min, max, numBuckets);
                return; // Skip the lock downgrade.
            }
            // Downgrade by acquiring read lock before releasing write lock
            mRwLock.readLock().lock();
        } finally {
            mRwLock.writeLock().unlock();
        }

        // Downgraded to read lock.
        // See base/android/java/src/org/chromium/base/metrics/forwarding_synchronization.md
        try {
            assert mDelegate != null;
            recordHistogramSampleAlreadyLocked(type, name, sample, min, max, numBuckets);
        } finally {
            mRwLock.readLock().unlock();
        }
    }

    /**
     * Tries to cache or record a histogram sample without creating a new {@link Histogram}.
     *
     * @param type histogram type.
     * @param name histogram name.
     * @param sample sample value.
     * @param min histogram min value.
     * @param max histogram max value.
     * @param numBuckets number of histogram buckets.
     * @return {@code false} if the sample needs to be recorded with a write lock.
     */
    private boolean tryAppendOrRecordSample(
            @Histogram.Type int type, String name, int sample, int min, int max, int numBuckets) {
        mRwLock.readLock().lock();
        try {
            if (mDelegate != null) {
                recordHistogramSampleAlreadyLocked(type, name, sample, min, max, numBuckets);
                return true;
            }
            Histogram histogram = mHistogramByName.get(name);
            if (histogram == null) {
                return false;
            }
            if (!histogram.addSample(type, name, sample, min, max, numBuckets)) {
                mDroppedHistogramSampleCount.incrementAndGet();
            }
            return true;
        } finally {
            mRwLock.readLock().unlock();
        }
    }

    /**
     * Appends a histogram {@code sample} to a cached {@link Histogram}. Creates the {@code
     * Histogram} if needed. Assumes that the <b>write lock</b> is held by the current thread.
     *
     * @param type histogram type.
     * @param name histogram name.
     * @param sample sample value.
     * @param min histogram min value.
     * @param max histogram max value.
     * @param numBuckets number of histogram buckets.
     */
    @GuardedBy("mRwLock")
    private void cacheHistogramSampleAlreadyWriteLocked(
            @Histogram.Type int type, String name, int sample, int min, int max, int numBuckets) {
        assert mRwLock.isWriteLockedByCurrentThread();
        Histogram histogram = mHistogramByName.get(name);
        if (histogram == null) {
            if (mHistogramByName.size() >= MAX_HISTOGRAM_COUNT) {
                // A cache filling up is most likely an indication of a bug.
                assert false : "Too many histograms in cache";
                mDroppedHistogramSampleCount.incrementAndGet();
                return;
            }
            histogram = new Histogram(type, name, min, max, numBuckets);
            mHistogramByName.put(name, histogram);
        }
        if (!histogram.addSample(type, name, sample, min, max, numBuckets)) {
            mDroppedHistogramSampleCount.incrementAndGet();
        }
    }

    /**
     * Forwards a histogram sample to the delegate. Assumes that a read lock is held by the current
     * thread. Shouldn't be called with a write lock held.
     *
     * @param type histogram type.
     * @param name histogram name.
     * @param sample sample value.
     * @param min histogram min value.
     * @param max histogram max value.
     * @param numBuckets number of histogram buckets.
     */
    @GuardedBy("mRwLock")
    private void recordHistogramSampleAlreadyLocked(
            @Histogram.Type int type, String name, int sample, int min, int max, int numBuckets) {
        assert mRwLock.getReadHoldCount() > 0;
        assert !mRwLock.isWriteLockedByCurrentThread();
        assert mDelegate != null : "recordSampleAlreadyLocked called with no delegate to record to";
        switch (type) {
            case Histogram.Type.BOOLEAN:
                mDelegate.recordBooleanHistogram(name, sample != 0);
                break;
            case Histogram.Type.EXPONENTIAL:
                mDelegate.recordExponentialHistogram(name, sample, min, max, numBuckets);
                break;
            case Histogram.Type.LINEAR:
                mDelegate.recordLinearHistogram(name, sample, min, max, numBuckets);
                break;
            case Histogram.Type.SPARSE:
                mDelegate.recordSparseHistogram(name, sample);
                break;
            default:
                throw new UnsupportedOperationException("Unknown histogram type " + type);
        }
    }

    @Override
    public void recordBooleanHistogram(String name, boolean boolSample) {
        final int sample = boolSample ? 1 : 0;
        final int min = 0;
        final int max = 0;
        final int numBuckets = 0;
        cacheOrRecordHistogramSample(Histogram.Type.BOOLEAN, name, sample, min, max, numBuckets);
    }

    @Override
    public void recordExponentialHistogram(
            String name, int sample, int min, int max, int numBuckets) {
        cacheOrRecordHistogramSample(
                Histogram.Type.EXPONENTIAL, name, sample, min, max, numBuckets);
    }

    @Override
    public void recordLinearHistogram(String name, int sample, int min, int max, int numBuckets) {
        cacheOrRecordHistogramSample(Histogram.Type.LINEAR, name, sample, min, max, numBuckets);
    }

    @Override
    public void recordSparseHistogram(String name, int sample) {
        final int min = 0;
        final int max = 0;
        final int numBuckets = 0;
        cacheOrRecordHistogramSample(Histogram.Type.SPARSE, name, sample, min, max, numBuckets);
    }

    @Override
    public void recordUserAction(String name, long elapsedRealtimeMillis) {
        mRwLock.readLock().lock();
        try {
            if (mDelegate != null) {
                mDelegate.recordUserAction(name, elapsedRealtimeMillis);
                return;
            }
        } finally {
            mRwLock.readLock().unlock();
        }

        mRwLock.writeLock().lock();
        try {
            if (mDelegate == null) {
                if (mUserActions.size() < MAX_USER_ACTION_COUNT) {
                    mUserActions.add(new UserAction(name, elapsedRealtimeMillis));
                } else {
                    assert false : "Too many user actions in cache";
                    mDroppedUserActionCount++;
                }
                if (mUserActionCallbacksForTesting != null) {
                    for (int i = 0; i < mUserActionCallbacksForTesting.size(); i++) {
                        mUserActionCallbacksForTesting.get(i).onResult(name);
                    }
                }
                return; // Skip the lock downgrade.
            }
            // Downgrade by acquiring read lock before releasing write lock
            mRwLock.readLock().lock();
        } finally {
            mRwLock.writeLock().unlock();
        }

        // Downgraded to read lock.
        // See base/android/java/src/org/chromium/base/metrics/forwarding_synchronization.md
        try {
            assert mDelegate != null;
            mDelegate.recordUserAction(name, elapsedRealtimeMillis);
        } finally {
            mRwLock.readLock().unlock();
        }
    }

    @VisibleForTesting
    @Override
    public int getHistogramValueCountForTesting(String name, int sample) {
        mRwLock.readLock().lock();
        try {
            if (mDelegate != null) return mDelegate.getHistogramValueCountForTesting(name, sample);

            Histogram histogram = mHistogramByName.get(name);
            if (histogram == null) return 0;
            int sampleCount = 0;
            synchronized (histogram) {
                for (int i = 0; i < histogram.mSamples.size(); i++) {
                    if (histogram.mSamples.get(i) == sample) sampleCount++;
                }
            }
            return sampleCount;
        } finally {
            mRwLock.readLock().unlock();
        }
    }

    @VisibleForTesting
    @Override
    public int getHistogramTotalCountForTesting(String name) {
        mRwLock.readLock().lock();
        try {
            if (mDelegate != null) return mDelegate.getHistogramTotalCountForTesting(name);

            Histogram histogram = mHistogramByName.get(name);
            if (histogram == null) return 0;
            synchronized (histogram) {
                return histogram.mSamples.size();
            }
        } finally {
            mRwLock.readLock().unlock();
        }
    }

    @VisibleForTesting
    @Override
    public List<HistogramBucket> getHistogramSamplesForTesting(String name) {
        mRwLock.readLock().lock();
        try {
            if (mDelegate != null) return mDelegate.getHistogramSamplesForTesting(name);

            Histogram histogram = mHistogramByName.get(name);
            if (histogram == null) return Collections.emptyList();
            Integer[] samplesCopy;
            synchronized (histogram) {
                samplesCopy = histogram.mSamples.toArray(new Integer[0]);
            }
            Arrays.sort(samplesCopy);
            List<HistogramBucket> buckets = new ArrayList<>();
            for (int i = 0; i < samplesCopy.length; ) {
                int value = samplesCopy[i];
                int countInBucket = 0;
                do {
                    countInBucket++;
                    i++;
                } while (i < samplesCopy.length && samplesCopy[i] == value);

                buckets.add(new HistogramBucket(value, value + 1, countInBucket));
            }
            return buckets;
        } finally {
            mRwLock.readLock().unlock();
        }
    }

    @VisibleForTesting
    @Override
    public void addUserActionCallbackForTesting(Callback<String> callback) {
        mRwLock.writeLock().lock();
        try {
            if (mUserActionCallbacksForTesting == null) {
                mUserActionCallbacksForTesting = new ArrayList<>();
            }
            mUserActionCallbacksForTesting.add(callback);
            if (mDelegate != null) mDelegate.addUserActionCallbackForTesting(callback);
        } finally {
            mRwLock.writeLock().unlock();
        }
    }

    @VisibleForTesting
    @Override
    public void removeUserActionCallbackForTesting(Callback<String> callback) {
        mRwLock.writeLock().lock();
        try {
            if (mUserActionCallbacksForTesting == null) {
                assert false
                        : "Attempting to remove a user action callback without previously "
                                + "registering any.";
                return;
            }
            mUserActionCallbacksForTesting.remove(callback);
            if (mDelegate != null) mDelegate.removeUserActionCallbackForTesting(callback);
        } finally {
            mRwLock.writeLock().unlock();
        }
    }

    @SuppressLint("VisibleForTests")
    @GuardedBy("mRwLock")
    private void swapUserActionCallbacksForTesting(
            @Nullable UmaRecorder previousRecorder, @Nullable UmaRecorder newRecorder) {
        if (mUserActionCallbacksForTesting == null) return;

        for (int i = 0; i < mUserActionCallbacksForTesting.size(); i++) {
            if (previousRecorder != null) {
                previousRecorder.removeUserActionCallbackForTesting(
                        mUserActionCallbacksForTesting.get(i));
            }
            if (newRecorder != null) {
                newRecorder.addUserActionCallbackForTesting(mUserActionCallbacksForTesting.get(i));
            }
        }
    }
}
