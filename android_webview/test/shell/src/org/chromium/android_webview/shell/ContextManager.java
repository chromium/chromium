// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.shell;

import android.view.Surface;

import org.chromium.base.annotations.JNINamespace;

/** draw_fn framework side implementation for tests. */
@JNINamespace("draw_fn")
public class ContextManager {
    private Surface mCurrentSurface;

    public static long getDrawFnFunctionTable() {
        return nativeGetDrawFnFunctionTable();
    }

    private final long mNativeContextManager;

    public ContextManager() {
        mNativeContextManager = nativeInit();
    }

    public void setSurface(Surface surface) {
        if (mCurrentSurface == surface) return;
        mCurrentSurface = surface;
        nativeSetSurface(mNativeContextManager, surface);
    }

    public void sync(int functor, boolean applyForceDark) {
        nativeSync(mNativeContextManager, functor, applyForceDark);
    }

    // Uses functor from last sync.
    public int[] draw(int width, int height, int scrollX, int scrollY, boolean readbackQuadrants) {
        return nativeDraw(
                mNativeContextManager, width, height, scrollX, scrollY, readbackQuadrants);
    }

    private static native long nativeGetDrawFnFunctionTable();
    private static native long nativeInit();
    private static native void nativeSetSurface(long nativeContextManager, Surface surface);
    private static native void nativeSync(
            long nativeContextManager, int functor, boolean applyForceDark);
    private static native int[] nativeDraw(long nativeContextManager, int width, int height,
            int scrollX, int scrollY, boolean readbackQuadrants);
}
