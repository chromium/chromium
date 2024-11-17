// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import androidx.annotation.Nullable;

import org.jni_zero.CalledByNative;

import java.util.Optional;

/**
 * A simple single-argument callback to handle the result of a computation.
 *
 * @param <T> The type of the computation's result.
 */
@FunctionalInterface
public interface Callback<T> {

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
    static <T> void runNullSafe(@Nullable Callback<T> callback, @Nullable T object) {
        if (callback != null) callback.onResult(object);
    }

    /**
     * JNI Generator does not know how to target static methods on interfaces
     * (which is new in Java 8, and requires desugaring).
     */
    abstract class Helper {
        @SuppressWarnings("unchecked")
        @CalledByNative("Helper")
        static void onObjectResultFromNative(Callback callback, Object result) {
            callback.onResult(result);
        }

        @SuppressWarnings("unchecked")
        @CalledByNative("Helper")
        static void onOptionalStringResultFromNative(
                Callback<Optional<String>> callback, boolean hasValue, String result) {
            callback.onResult(hasValue ? Optional.of(result) : Optional.empty());
        }

        @SuppressWarnings("unchecked")
        @CalledByNative("Helper")
        static void onBooleanResultFromNative(Callback callback, boolean result) {
            callback.onResult(Boolean.valueOf(result));
        }

        @SuppressWarnings("unchecked")
        @CalledByNative("Helper")
        static void onIntResultFromNative(Callback callback, int result) {
            callback.onResult(Integer.valueOf(result));
        }

        @SuppressWarnings("unchecked")
        @CalledByNative("Helper")
        static void onLongResultFromNative(Callback callback, long result) {
            callback.onResult(Long.valueOf(result));
        }

        @SuppressWarnings("unchecked")
        @CalledByNative("Helper")
        static void onTimeResultFromNative(Callback callback, long result) {
            callback.onResult(Long.valueOf(result));
        }

        @CalledByNative("Helper")
        static void runRunnable(Runnable runnable) {
            runnable.run();
        }
    }
}
