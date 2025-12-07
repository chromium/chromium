// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.process_launcher;

import android.os.Handler;

import com.google.errorprone.annotations.MustBeClosed;

import org.jni_zero.CalledByNative;

import org.chromium.base.BindingRequestQueue;
import org.chromium.build.annotations.Nullable;

/**
 * ScopedServiceBindingBatch is used to batch up service binding requests.
 *
 * <p>When a ScopedServiceBindingBatch is created, it begins a batch update on the process launcher
 * thread. When the ScopedServiceBindingBatch is closed, it ends the batch update.
 * ScopedServiceBindingBatch supports nested batch updates. If the batch update count drops to 0,
 * the binding request queue is flushed.
 *
 * <p>This also records the duration of the batching scope.
 */
public interface ScopedServiceBindingBatch extends AutoCloseable {
    /**
     * Returns a ScopedServiceBindingBatch.
     *
     * <p>If the feature was previously activated (via tryActivate()): Returns an instance that will
     * cause bindings to be batched until close() is called. Nested calls are supported.
     *
     * <p>If the feature was not activated: Return an instance that just acts as a timer. Nested
     * calls will return null.
     *
     * <p>Must be called on the same thread as the thread that called tryActivate(). This is usually
     * the UI thread.
     */
    @CalledByNative
    @MustBeClosed
    static @Nullable ScopedServiceBindingBatch scoped() {
        ScopedServiceBindingBatchImpl realBatch = ScopedServiceBindingBatchImpl.scoped();
        if (realBatch != null) {
            return realBatch;
        }
        return ScopedServiceBindingBatchDuration.tryMeasure();
    }

    /**
     * Try to activate the feature if possible.
     *
     * <p>This must be called before using {@link #scoped()}. This must be called on the main thread
     * and only once.
     *
     * <p>If required feature is not enabled, or if required API is not available, this returns
     * false. {@link #scoped()} will return null or ScopedServiceBindingBatchDuration in this case.
     *
     * @param launcherHandler The handler to use for posting tasks to the process launcher thread.
     */
    static boolean tryActivate(Handler launcherHandler) {
        return ScopedServiceBindingBatchImpl.tryActivate(launcherHandler);
    }

    /**
     * Returns whether a batch update is in progress.
     *
     * <p>This must be called on the process launcher thread.
     */
    static boolean shouldBatchUpdate() {
        return ScopedServiceBindingBatchImpl.shouldBatchUpdate();
    }

    /**
     * Returns the binding request queue.
     *
     * <p>This must be called on the process launcher thread while shouldBatchUpdate() is true.
     */
    static BindingRequestQueue getBindingRequestQueue() {
        return ScopedServiceBindingBatchImpl.getBindingRequestQueue();
    }

    @CalledByNative
    @Override
    void close();
}
