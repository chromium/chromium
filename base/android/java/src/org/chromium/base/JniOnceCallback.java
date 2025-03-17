// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

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
 */
@JNINamespace("base::android")
@NullMarked
public final class JniOnceCallback<T extends @Nullable Object> implements Callback<T> {
    private final @Nullable LifetimeAssert mLifetimeAssert = LifetimeAssert.create(this);
    long mNativePointer;

    @CalledByNative
    private JniOnceCallback(long nativePointer) {
        mNativePointer = nativePointer;
    }

    @Override
    public void onResult(T result) {
        if (mNativePointer != 0) {
            JniCallbackUtils.runNativeCallback(this, result);
            mNativePointer = 0;
            LifetimeAssert.setSafeToGc(mLifetimeAssert, true);
        } else {
            // TODO(mheikal): maybe store destroy callstack to output here?
            assert false : "Called destroyed callback";
        }
    }

    /** Frees the owned base::OnceCallback's memory */
    public void destroy() {
        if (mNativePointer != 0) {
            JniCallbackUtils.destroyNativeCallback(this);
            mNativePointer = 0;
        }
        LifetimeAssert.setSafeToGc(mLifetimeAssert, true);
    }
}
