// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.lifetime.LifetimeAssert;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/*
 * A wrapper that owns a native side base::OnceCallback.
 *
 * You must call JniOnceCallback#destroy() if you never end up calling onResult
 * so as to not leak the native callback.
 *
 * This class has no additional thread safety measures compared to
 * base::RepeatingCallback.
 *
 * Class is package-private in order to force clients to use one of the implemented
 * interfaces (to reduce confusion of a Callback that is also a Runnable), and to
 * better document Once vs Repeated semantics.
 */
@JNINamespace("base::android")
@NullMarked
final class JniCallbackImpl
        implements JniOnceCallback<Object>,
                JniOnceRunnable,
                JniRepeatingCallback<Object>,
                JniRepeatingRunnable,
                JniOnceCallback2<Object, Object>,
                JniRepeatingCallback2<Object, Object> {
    private final @Nullable LifetimeAssert mLifetimeAssert = LifetimeAssert.create(this);
    private long mNativePointer;
    private final boolean mIsRepeating;

    @CalledByNative
    private JniCallbackImpl(boolean isRepeating, long nativePointer) {
        mIsRepeating = isRepeating;
        mNativePointer = nativePointer;
    }

    @Override
    @SuppressWarnings("NullAway")
    public void run() {
        // When used as a Runnable instead of a Callback<T>, the JNI call still expects a
        // parameter, but discards it and calls the no-arg OnceCallback. This was simpler
        // than introducing a second JNI call that does not take a parameter.
        onResult(null);
    }

    @Override
    public void onResult(Object result) {
        // Exception rather than assert since this sort of error often happens in low-frequency
        // edge cases.
        if (mNativePointer == 0) {
            throw new NullPointerException();
        }
        JniCallbackImplJni.get().onResult(mIsRepeating, mNativePointer, result);
        if (!mIsRepeating) {
            mNativePointer = 0;
            LifetimeAssert.destroy(mLifetimeAssert);
        }
    }

    @Override
    public void onResult(@Nullable Object result1, @Nullable Object result2) {
        if (mNativePointer == 0) {
            throw new NullPointerException();
        }
        JniCallbackImplJni.get().onResult2(mIsRepeating, mNativePointer, result1, result2);
        if (!mIsRepeating) {
            mNativePointer = 0;
            LifetimeAssert.destroy(mLifetimeAssert);
        }
    }

    /** Frees the owned base::OnceCallback's memory */
    @Override
    public void destroy() {
        if (mNativePointer != 0) {
            JniCallbackImplJni.get().destroy(mIsRepeating, mNativePointer);
            mNativePointer = 0;
            LifetimeAssert.destroy(mLifetimeAssert);
        }
    }

    @NativeMethods
    interface Natives {
        void onResult(boolean isRepeating, long callbackPtr, @Nullable Object result);

        void onResult2(
                boolean isRepeating,
                long callbackPtr,
                @Nullable Object result1,
                @Nullable Object result2);

        void destroy(boolean isRepeating, long callbackPtr);
    }
}
