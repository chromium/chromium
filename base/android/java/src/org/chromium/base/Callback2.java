// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import org.jni_zero.CalledByNative;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * A simple two-argument callback to handle the result of a computation.
 *
 * @param <T1> The type of the first result.
 * @param <T2> The type of the second result.
 */
@NullMarked
@FunctionalInterface
public interface Callback2<T1 extends @Nullable Object, T2 extends @Nullable Object> {
    /** Invoked with the results of a computation. */
    void onResult(T1 result1, T2 result2);

    abstract class JniHelper {
        @CalledByNative("JniHelper")
        static void onResultFromNative(Callback2<Object, Object> callback, Object r1, Object r2) {
            callback.onResult(r1, r2);
        }
    }
}
