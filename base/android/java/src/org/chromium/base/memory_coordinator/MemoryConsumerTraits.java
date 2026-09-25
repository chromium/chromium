// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.memory_coordinator;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Describes how a {@link MemoryConsumer} behaves using a set of trait enum values. */
@JNINamespace("base::android")
@NullMarked
public final class MemoryConsumerTraits {
    // LINT.IfChange(MemoryConsumerTraits)
    private final @ConsumerType int mConsumerType;
    private final @EstimatedMemoryUsage int mEstimatedMemoryUsage;
    private final @ReleaseMemoryCost int mReleaseMemoryCost;
    private final @InformationRetention int mInformationRetention;
    private final @ExecutionType int mExecutionType;
    private final @SupportsMemoryLimit int mSupportsMemoryLimit;
    private final @InProcess int mInProcess;
    private final @RecreateMemoryCost int mRecreateMemoryCost;
    private final @ReleaseGCReferences int mReleaseGCReferences;
    private final @GarbageCollectsV8Heap int mGarbageCollectsV8Heap;
    private final @IsStateful int mIsStateful;

    private MemoryConsumerTraits(
            @ConsumerType int consumerType,
            @EstimatedMemoryUsage int estimatedMemoryUsage,
            @ReleaseMemoryCost int releaseMemoryCost,
            @InformationRetention int informationRetention,
            @ExecutionType int executionType,
            @SupportsMemoryLimit int supportsMemoryLimit,
            @InProcess int inProcess,
            @RecreateMemoryCost int recreateMemoryCost,
            @ReleaseGCReferences int releaseGCReferences,
            @GarbageCollectsV8Heap int garbageCollectsV8Heap,
            @IsStateful int isStateful) {
        mConsumerType = consumerType;
        mEstimatedMemoryUsage = estimatedMemoryUsage;
        mReleaseMemoryCost = releaseMemoryCost;
        mInformationRetention = informationRetention;
        mExecutionType = executionType;
        mSupportsMemoryLimit = supportsMemoryLimit;
        mInProcess = inProcess;
        mRecreateMemoryCost = recreateMemoryCost;
        mReleaseGCReferences = releaseGCReferences;
        mGarbageCollectsV8Heap = garbageCollectsV8Heap;
        mIsStateful = isStateful;
    }

    /** Creates default passive traits. */
    public static MemoryConsumerTraits createPassive() {
        return createPassive(SupportsMemoryLimit.YES, InProcess.NA, ReleaseGCReferences.NO);
    }

    /** Creates passive traits with customized optional values. */
    public static MemoryConsumerTraits createPassive(
            @SupportsMemoryLimit int supportsMemoryLimit,
            @InProcess int inProcess,
            @ReleaseGCReferences int releaseGCReferences) {
        return new MemoryConsumerTraits(
                ConsumerType.PASSIVE,
                EstimatedMemoryUsage.NA,
                ReleaseMemoryCost.NA,
                InformationRetention.NA,
                ExecutionType.SYNCHRONOUS,
                supportsMemoryLimit,
                inProcess,
                RecreateMemoryCost.NA,
                releaseGCReferences,
                GarbageCollectsV8Heap.NO,
                IsStateful.YES);
    }

    /** Returns all traits packed into a single 32-bit integer for efficient JNI transfer. */
    @CalledByNative
    public int getPackedTraits() {
        return (mConsumerType & 0x1)
                | ((mEstimatedMemoryUsage & 0x3) << 1)
                | ((mReleaseMemoryCost & 0x3) << 3)
                | ((mInformationRetention & 0x3) << 5)
                | ((mExecutionType & 0x1) << 7)
                | ((mSupportsMemoryLimit & 0x1) << 8)
                | ((mInProcess & 0x3) << 9)
                | ((mRecreateMemoryCost & 0x3) << 11)
                | ((mReleaseGCReferences & 0x1) << 13)
                | ((mGarbageCollectsV8Heap & 0x1) << 14)
                | ((mIsStateful & 0x1) << 15);
    }

    // LINT.ThenChange(//base/memory_coordinator/memory_consumer_android.cc)

    public @ConsumerType int getConsumerType() {
        return mConsumerType;
    }

    public @EstimatedMemoryUsage int getEstimatedMemoryUsage() {
        return mEstimatedMemoryUsage;
    }

    public @ReleaseMemoryCost int getReleaseMemoryCost() {
        return mReleaseMemoryCost;
    }

    public @InformationRetention int getInformationRetention() {
        return mInformationRetention;
    }

    public @ExecutionType int getExecutionType() {
        return mExecutionType;
    }

    public @SupportsMemoryLimit int getSupportsMemoryLimit() {
        return mSupportsMemoryLimit;
    }

    public @InProcess int getInProcess() {
        return mInProcess;
    }

    public @RecreateMemoryCost int getRecreateMemoryCost() {
        return mRecreateMemoryCost;
    }

    public @ReleaseGCReferences int getReleaseGCReferences() {
        return mReleaseGCReferences;
    }

    public @GarbageCollectsV8Heap int getGarbageCollectsV8Heap() {
        return mGarbageCollectsV8Heap;
    }

    public @IsStateful int getIsStateful() {
        return mIsStateful;
    }

    @Override
    public boolean equals(@Nullable Object obj) {
        if (this == obj) return true;
        if (!(obj instanceof MemoryConsumerTraits)) return false;
        MemoryConsumerTraits other = (MemoryConsumerTraits) obj;
        return mConsumerType == other.mConsumerType
                && mEstimatedMemoryUsage == other.mEstimatedMemoryUsage
                && mReleaseMemoryCost == other.mReleaseMemoryCost
                && mInformationRetention == other.mInformationRetention
                && mExecutionType == other.mExecutionType
                && mSupportsMemoryLimit == other.mSupportsMemoryLimit
                && mInProcess == other.mInProcess
                && mRecreateMemoryCost == other.mRecreateMemoryCost
                && mReleaseGCReferences == other.mReleaseGCReferences
                && mGarbageCollectsV8Heap == other.mGarbageCollectsV8Heap
                && mIsStateful == other.mIsStateful;
    }

    @Override
    public int hashCode() {
        return Integer.hashCode(getPackedTraits());
    }

    @Override
    public String toString() {
        return "MemoryConsumerTraits{"
                + "consumerType="
                + mConsumerType
                + ", estimatedMemoryUsage="
                + mEstimatedMemoryUsage
                + ", releaseMemoryCost="
                + mReleaseMemoryCost
                + ", informationRetention="
                + mInformationRetention
                + ", executionType="
                + mExecutionType
                + ", supportsMemoryLimit="
                + mSupportsMemoryLimit
                + ", inProcess="
                + mInProcess
                + ", recreateMemoryCost="
                + mRecreateMemoryCost
                + ", releaseGCReferences="
                + mReleaseGCReferences
                + ", garbageCollectsV8Heap="
                + mGarbageCollectsV8Heap
                + ", isStateful="
                + mIsStateful
                + '}';
    }

    /** Builder for active {@link MemoryConsumerTraits}. */
    public static final class Builder {
        private final @EstimatedMemoryUsage int mEstimatedMemoryUsage;
        private final @ReleaseMemoryCost int mReleaseMemoryCost;
        private final @InformationRetention int mInformationRetention;
        private final @ExecutionType int mExecutionType;

        private @SupportsMemoryLimit int mSupportsMemoryLimit = SupportsMemoryLimit.YES;
        private @InProcess int mInProcess = InProcess.YES;
        private @RecreateMemoryCost int mRecreateMemoryCost = RecreateMemoryCost.NA;
        private @ReleaseGCReferences int mReleaseGCReferences = ReleaseGCReferences.NO;
        private @GarbageCollectsV8Heap int mGarbageCollectsV8Heap = GarbageCollectsV8Heap.NO;
        private @IsStateful int mIsStateful = IsStateful.YES;

        public Builder(
                @EstimatedMemoryUsage int estimatedMemoryUsage,
                @ReleaseMemoryCost int releaseMemoryCost,
                @InformationRetention int informationRetention,
                @ExecutionType int executionType) {
            mEstimatedMemoryUsage = estimatedMemoryUsage;
            mReleaseMemoryCost = releaseMemoryCost;
            mInformationRetention = informationRetention;
            mExecutionType = executionType;
        }

        public Builder setSupportsMemoryLimit(@SupportsMemoryLimit int supportsMemoryLimit) {
            mSupportsMemoryLimit = supportsMemoryLimit;
            return this;
        }

        public Builder setInProcess(@InProcess int inProcess) {
            mInProcess = inProcess;
            return this;
        }

        public Builder setRecreateMemoryCost(@RecreateMemoryCost int recreateMemoryCost) {
            mRecreateMemoryCost = recreateMemoryCost;
            return this;
        }

        public Builder setReleaseGCReferences(@ReleaseGCReferences int releaseGCReferences) {
            mReleaseGCReferences = releaseGCReferences;
            return this;
        }

        public Builder setGarbageCollectsV8Heap(@GarbageCollectsV8Heap int garbageCollectsV8Heap) {
            mGarbageCollectsV8Heap = garbageCollectsV8Heap;
            return this;
        }

        public Builder setIsStateful(@IsStateful int isStateful) {
            mIsStateful = isStateful;
            return this;
        }

        public MemoryConsumerTraits build() {
            return new MemoryConsumerTraits(
                    ConsumerType.ACTIVE,
                    mEstimatedMemoryUsage,
                    mReleaseMemoryCost,
                    mInformationRetention,
                    mExecutionType,
                    mSupportsMemoryLimit,
                    mInProcess,
                    mRecreateMemoryCost,
                    mReleaseGCReferences,
                    mGarbageCollectsV8Heap,
                    mIsStateful);
        }
    }
}
