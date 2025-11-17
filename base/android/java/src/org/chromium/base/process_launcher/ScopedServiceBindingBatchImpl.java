// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.process_launcher;

import android.os.Handler;
import android.os.Looper;
import android.os.MessageQueue;

import org.chromium.base.AconfigFlaggedApiDelegate;
import org.chromium.base.BaseFeatureList;
import org.chromium.base.BindingRequestQueue;
import org.chromium.base.TraceEvent;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * ScopedServiceBindingBatchImpl is used to batch up service binding requests.
 *
 * <p>When a ScopedServiceBindingBatchImpl is created, it begins a batch update on the process
 * launcher thread. When the ScopedServiceBindingBatchImpl is closed, it ends the batch update.
 * ScopedServiceBindingBatchImpl supports nested batch updates. If the batch update count drops to
 * 0, the binding request queue is flushed.
 *
 * <p>ScopedServiceBindingBatchImpl.scoped() and its close() method must be called on the main
 * thread to ensure that nested batch window does not partially overlap. The batch open/end events
 * are dispatched to the process launcher thread and counter is incremented/decremented on the
 * launcher thread.
 *
 * <p>While it is in batch mode, BindService will queue up binding requests. When the batch is over,
 * the queue is flushed.
 */
@NullMarked
/*package*/ final class ScopedServiceBindingBatchImpl implements ScopedServiceBindingBatch {
    // Arbitrary trace id which does not overlap the PID range.
    private static final long TRACE_ID_ON_MAIN_THREAD = (1 << 22) + 1;
    private static final long TRACE_ID_ON_LAUNCHER_THREAD = TRACE_ID_ON_MAIN_THREAD + 1;

    private static @Nullable BindingRequestQueue sBindingRequestQueueForTesting;
    // If the ScopedServiceBindingBatch feature is activated, sContextHolder is non-null.
    private static volatile @Nullable ContextHolder sContextHolder;

    private static final class ContextHolder {
        int mBatchUpdateCount;
        final Looper mCallingThreadLooper;
        final Handler mLauncherHandler;
        final BindingRequestQueue mBindingRequestQueue;

        ContextHolder(
                Looper callingThreadLooper,
                Handler launcherHandler,
                BindingRequestQueue bindingRequestQueue) {
            mCallingThreadLooper = callingThreadLooper;
            mLauncherHandler = launcherHandler;
            mBindingRequestQueue = bindingRequestQueue;
        }
    }

    private final ContextHolder mContextHolder;
    private final @Nullable ScopedServiceBindingBatchDuration mDuration;

    private ScopedServiceBindingBatchImpl(ContextHolder contextHolder) {
        assert contextHolder.mCallingThreadLooper == Looper.myLooper();

        mContextHolder = contextHolder;
        mDuration = ScopedServiceBindingBatchDuration.tryMeasure();
        TraceEvent.startAsync("ScopedServiceBindingBatch", TRACE_ID_ON_MAIN_THREAD);
        contextHolder.mLauncherHandler.post(this::beginOnLauncherThread);
    }

    /**
     * Returns a ScopedServiceBindingBatchImpl if the feature is activated, otherwise returns null.
     */
    static @Nullable ScopedServiceBindingBatchImpl scoped() {
        ContextHolder contextHolder = sContextHolder;
        if (contextHolder == null) {
            return null;
        }
        return new ScopedServiceBindingBatchImpl(contextHolder);
    }

    static void setBindingRequestQueueForTesting(@Nullable BindingRequestQueue queue) {
        sBindingRequestQueueForTesting = queue;
    }

    static void clearContextForTesting() {
        sContextHolder = null;
    }

    static boolean tryActivate(Handler launcherHandler) {
        // BaseFeatureList.sRebindingChildServiceConnectionController.isEnabled() is checked instead
        // of RebindingChildServiceConnectionController.isEnabled() to bypass
        // AconfigFlaggedApiDelegate check in test.
        boolean isFeatureEnabled =
                BaseFeatureList.sEffectiveBindingState.isEnabled()
                        && BaseFeatureList.sRebindingChildServiceConnectionController.isEnabled()
                        && BaseFeatureList.sRebindServiceBatchApi.isEnabled();
        if (!isFeatureEnabled) {
            return false;
        }
        assert sContextHolder == null : "ScopedServiceBindingBatchImpl was already activated.";

        BindingRequestQueue queue;
        if (sBindingRequestQueueForTesting == null) {
            AconfigFlaggedApiDelegate delegate = AconfigFlaggedApiDelegate.getInstance();
            if (delegate == null || !delegate.isUpdateServiceBindingApiAvailable()) {
                return false;
            }
            queue = delegate.getBindingRequestQueue();
            if (queue == null) {
                return false;
            }
        } else {
            queue = sBindingRequestQueueForTesting;
        }
        Looper callingThreadLooper = Looper.myLooper();
        if (callingThreadLooper == null) {
            return false;
        }
        sContextHolder = new ContextHolder(callingThreadLooper, launcherHandler, queue);
        if (BaseFeatureList.sRebindServiceBatchApiFlushOnIdle.getValue()) {
            launcherHandler
                    .getLooper()
                    .getQueue()
                    .addIdleHandler(
                            new MessageQueue.IdleHandler() {
                                @Override
                                public boolean queueIdle() {
                                    if (shouldBatchUpdate()) {
                                        queue.flush();
                                    }
                                    return true;
                                }
                            });
        }
        return true;
    }

    /**
     * Returns whether a batch update is in progress.
     *
     * <p>This must be called on the process launcher thread.
     */
    static boolean shouldBatchUpdate() {
        ContextHolder contextHolder = sContextHolder;
        if (contextHolder == null) {
            return false;
        }
        assert contextHolder.mLauncherHandler.getLooper() == Looper.myLooper();
        return contextHolder.mBatchUpdateCount > 0;
    }

    /**
     * Returns the binding request queue.
     *
     * <p>This must be called on the process launcher thread while shouldBatchUpdate() is true.
     */
    static BindingRequestQueue getBindingRequestQueue() {
        ContextHolder contextHolder = sContextHolder;
        assert contextHolder != null;
        assert contextHolder.mLauncherHandler.getLooper() == Looper.myLooper();
        return contextHolder.mBindingRequestQueue;
    }

    @Override
    public void close() {
        assert mContextHolder.mCallingThreadLooper == Looper.myLooper();
        mContextHolder.mLauncherHandler.post(this::endOnLauncherThread);
        TraceEvent.finishAsync("ScopedServiceBindingBatch", TRACE_ID_ON_MAIN_THREAD);
        if (mDuration != null) {
            mDuration.close();
        }
    }

    private void beginOnLauncherThread() {
        TraceEvent.startAsync(
                "ScopedServiceBindingBatchOnLauncherThread", TRACE_ID_ON_LAUNCHER_THREAD);
        mContextHolder.mBatchUpdateCount++;
    }

    private void endOnLauncherThread() {
        mContextHolder.mBatchUpdateCount--;
        if (mContextHolder.mBatchUpdateCount == 0) {
            mContextHolder.mBindingRequestQueue.flush();
        }
        TraceEvent.finishAsync(
                "ScopedServiceBindingBatchOnLauncherThread", TRACE_ID_ON_LAUNCHER_THREAD);
    }
}
