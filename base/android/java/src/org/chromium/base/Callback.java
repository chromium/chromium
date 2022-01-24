// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import org.chromium.base.annotations.CalledByNative;

/**
 * A simple single-argument callback to handle the result of a computation.
 *
 * @param <T> The type of the computation's result.
 */
@FunctionalInterface
public interface Callback<T> {
    /**
     * Invoked with the result of a computation.
     */
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
        static void onTimeResultFromNative(Callback callback, long result) {
            callback.onResult(Long.valueOf(result));
        }

        @CalledByNative("Helper")
        static void runRunnable(Runnable runnable) {
            runnable.run();
        }
    }
}
