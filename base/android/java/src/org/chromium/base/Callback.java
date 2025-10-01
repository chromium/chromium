// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import org.jni_zero.CalledByNative;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.NullUnmarked;
import org.chromium.build.annotations.Nullable;

/**
 * A simple single-argument callback to handle the result of a computation.
 *
 * @param <T> The type of the computation's result.
 */
@NullMarked
@FunctionalInterface
public interface Callback<T extends @Nullable Object> {

    /** Invoked with the result of a computation. */
    void onResult(T result);

    /**
     * Returns a Runnable that will invoke the callback with the given value.
     *
     * For example, instead of:
     *     mView.post(() -> myCallback.onResult(result));
     * Avoid creating an inner class via:
     *     mView.post(myCallback.bind(result));
     */
    default Runnable bind(T result) {
        return () -> onResult(result);
    }

    /**
     * Runs a callback checking if the callback may be null.
     *
     * <p>Can be used as syntactic sugar for: if (callback != null) callback.onResult(object);
     *
     * @param callback The {@link Callback} to run.
     * @param object The payload to provide to the callback (may be null).
     */
    @NullUnmarked // https://github.com/uber/NullAway/issues/1075
    static <T extends @Nullable Object> void runNullSafe(@Nullable Callback<T> callback, T object) {
        if (callback != null) callback.onResult(object);
    }

    // TODO(agrieve): Wrapper can be removed once min_supported_sdk_version >= 24.
    abstract class Helper {
        @CalledByNative("Helper")
        static void onObjectResultFromNative(Callback<Object> callback, Object result) {
            callback.onResult(result);
        }

        @CalledByNative("Helper")
        static void onBooleanResultFromNative(Callback<Boolean> callback, boolean result) {
            callback.onResult(result);
        }

        @CalledByNative("Helper")
        static void onIntResultFromNative(Callback<Integer> callback, int result) {
            callback.onResult(result);
        }

        @CalledByNative("Helper")
        static void onLongResultFromNative(Callback<Long> callback, long result) {
            callback.onResult(result);
        }

        @CalledByNative("Helper")
        static void runRunnable(Runnable runnable) {
            runnable.run();
        }
    }
}
