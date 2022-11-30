// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.shell;

import android.view.Surface;

import org.chromium.base.annotations.JNINamespace;

/** draw_fn framework side implementation for tests. */
@JNINamespace("draw_fn")
public class ContextManager {
    private static boolean sUseVulkan;
    private Surface mCurrentSurface;

    public static long getDrawFnFunctionTable(boolean useVulkan) {
        sUseVulkan = useVulkan;
        return nativeGetDrawFnFunctionTable(useVulkan);
    }

    private final long mNativeContextManager;

    public ContextManager() {
        mNativeContextManager = nativeInit(sUseVulkan);
    }

    public void setSurface(Surface surface, int width, int height) {
        if (mCurrentSurface == surface) {
            if (surface != null) nativeResizeSurface(mNativeContextManager, width, height);
            return;
        }
        mCurrentSurface = surface;
        nativeSetSurface(mNativeContextManager, surface, width, height);
    }

    public void setOverlaysSurface(Surface surface) {
        nativeSetOverlaysSurface(mNativeContextManager, surface);
    }

    public void sync(int functor, boolean applyForceDark) {
        nativeSync(mNativeContextManager, functor, applyForceDark);
    }

    // Uses functor from last sync.
    public int[] draw(int width, int height, int scrollX, int scrollY, boolean readbackQuadrants) {
        return nativeDraw(
                mNativeContextManager, width, height, scrollX, scrollY, readbackQuadrants);
    }

    private static native long nativeGetDrawFnFunctionTable(boolean useVulkan);
    private static native long nativeInit(boolean useVulkan);
    private static native void nativeSetSurface(
            long nativeContextManager, Surface surface, int width, int height);
    private static native void nativeResizeSurface(
            long nativeContextManager, int width, int height);
    private static native void nativeSetOverlaysSurface(long nativeContextManager, Surface surface);
    private static native void nativeSync(
            long nativeContextManager, int functor, boolean applyForceDark);
    private static native int[] nativeDraw(long nativeContextManager, int width, int height,
            int scrollX, int scrollY, boolean readbackQuadrants);
}
