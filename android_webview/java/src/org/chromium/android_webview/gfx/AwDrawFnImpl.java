// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.gfx;

import android.graphics.Canvas;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.android_webview.common.Lifetime;

/** Implementation of draw_fn.h. */
@JNINamespace("android_webview")
@Lifetime.WebView
public class AwDrawFnImpl implements AwFunctor {
    private long mNativeAwDrawFnImpl;
    private final DrawFnAccess mAccess;
    private final int mHandle;

    /** Interface for inserting functor into canvas */
    public interface DrawFnAccess {
        void drawWebViewFunctor(Canvas canvas, int functor);
    }

    public AwDrawFnImpl(DrawFnAccess access) {
        mAccess = access;
        mNativeAwDrawFnImpl = AwDrawFnImplJni.get().create();
        mHandle = AwDrawFnImplJni.get().getFunctorHandle(mNativeAwDrawFnImpl, AwDrawFnImpl.this);
    }

    @Override
    public void destroy() {
        assert mNativeAwDrawFnImpl != 0;
        AwDrawFnImplJni.get().releaseHandle(mNativeAwDrawFnImpl, AwDrawFnImpl.this);
        // Native side is free to destroy itself after ReleaseHandle.
        mNativeAwDrawFnImpl = 0;
    }

    public static void setDrawFnFunctionTable(long functionTablePointer) {
        AwDrawFnImplJni.get().setDrawFnFunctionTable(functionTablePointer);
    }

    @Override
    public long getNativeCompositorFrameConsumer() {
        assert mNativeAwDrawFnImpl != 0;
        return AwDrawFnImplJni.get()
                .getCompositorFrameConsumer(mNativeAwDrawFnImpl, AwDrawFnImpl.this);
    }

    @Override
    public boolean requestDraw(Canvas canvas) {
        assert mNativeAwDrawFnImpl != 0;
        mAccess.drawWebViewFunctor(canvas, mHandle);
        return true;
    }

    @Override
    public void trimMemory() {}

    @NativeMethods
    interface Natives {
        int getFunctorHandle(long nativeAwDrawFnImpl, AwDrawFnImpl caller);

        long getCompositorFrameConsumer(long nativeAwDrawFnImpl, AwDrawFnImpl caller);

        void releaseHandle(long nativeAwDrawFnImpl, AwDrawFnImpl caller);

        void setDrawFnFunctionTable(long functionTablePointer);

        long create();
    }
}
