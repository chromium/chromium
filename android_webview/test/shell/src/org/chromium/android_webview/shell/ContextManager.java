// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.shell;

import android.view.Surface;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

/** draw_fn framework side implementation for tests. */
@JNINamespace("draw_fn")
public class ContextManager {
    private static boolean sUseVulkan;
    private Surface mCurrentSurface;

    public static long getDrawFnFunctionTable(boolean useVulkan) {
        sUseVulkan = useVulkan;
        return ContextManagerJni.get().getDrawFnFunctionTable(useVulkan);
    }

    private final long mNativeContextManager;

    public ContextManager() {
        mNativeContextManager = ContextManagerJni.get().init(sUseVulkan);
    }

    public void setSurface(Surface surface, int width, int height) {
        if (mCurrentSurface == surface) {
            if (surface != null) {
                ContextManagerJni.get().resizeSurface(mNativeContextManager, width, height);
            }
            return;
        }
        mCurrentSurface = surface;
        ContextManagerJni.get().setSurface(mNativeContextManager, surface, width, height);
    }

    public void setOverlaysSurface(Surface surface) {
        ContextManagerJni.get().setOverlaysSurface(mNativeContextManager, surface);
    }

    public void sync(int functor, boolean applyForceDark) {
        ContextManagerJni.get().sync(mNativeContextManager, functor, applyForceDark);
    }

    // Uses functor from last sync.
    public int[] draw(int width, int height, int scrollX, int scrollY, boolean readbackQuadrants) {
        return ContextManagerJni.get()
                .draw(mNativeContextManager, width, height, scrollX, scrollY, readbackQuadrants);
    }

    @NativeMethods
    interface Natives {
        long getDrawFnFunctionTable(boolean useVulkan);

        long init(boolean useVulkan);

        void setSurface(long nativeContextManager, Surface surface, int width, int height);

        void resizeSurface(long nativeContextManager, int width, int height);

        void setOverlaysSurface(long nativeContextManager, Surface surface);

        void sync(long nativeContextManager, int functor, boolean applyForceDark);

        int[] draw(
                long nativeContextManager,
                int width,
                int height,
                int scrollX,
                int scrollY,
                boolean readbackQuadrants);
    }
}
