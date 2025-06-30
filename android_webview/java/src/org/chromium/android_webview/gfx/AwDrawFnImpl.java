// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.gfx;

import android.graphics.Canvas;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.android_webview.common.Lifetime;
import org.chromium.build.annotations.NullMarked;

/** Implementation of draw_fn.h. */
@JNINamespace("android_webview")
@Lifetime.WebView
@NullMarked
public final class AwDrawFnImpl {
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
        mHandle = AwDrawFnImplJni.get().getFunctorHandle(mNativeAwDrawFnImpl);
    }

    /** Destroy on UI thread. Client should stop using CompositorFrameConsumer before this */
    public void destroy() {
        assert mNativeAwDrawFnImpl != 0;
        AwDrawFnImplJni.get().releaseHandle(mNativeAwDrawFnImpl);
        // Native side is free to destroy itself after ReleaseHandle.
        mNativeAwDrawFnImpl = 0;
    }

    public static void setDrawFnFunctionTable(long functionTablePointer) {
        AwDrawFnImplJni.get().setDrawFnFunctionTable(functionTablePointer);
    }

    /** Return the raw native pointer to CompositorFrameConsumer */
    public long getNativeCompositorFrameConsumer() {
        assert mNativeAwDrawFnImpl != 0;
        return AwDrawFnImplJni.get().getCompositorFrameConsumer(mNativeAwDrawFnImpl);
    }

    /** Insert draw functor into recording canvas */
    public boolean requestDraw(Canvas canvas) {
        assert mNativeAwDrawFnImpl != 0;
        mAccess.drawWebViewFunctor(canvas, mHandle);
        return true;
    }

    /**
     * Intended for test code.
     *
     * @return the number of references from WebView to this class. The remaining references are
     *     from Android libhwui.
     */
    @VisibleForTesting
    public static int getReferenceInstanceCount() {
        return AwDrawFnImplJni.get().getReferenceInstanceCount();
    }

    @NativeMethods
    interface Natives {
        int getFunctorHandle(long nativeAwDrawFnImpl);

        long getCompositorFrameConsumer(long nativeAwDrawFnImpl);

        void releaseHandle(long nativeAwDrawFnImpl);

        void setDrawFnFunctionTable(long functionTablePointer);

        long create();

        int getReferenceInstanceCount();
    }
}
