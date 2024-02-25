// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.scene_layer;

import androidx.annotation.ColorInt;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.chrome.browser.layouts.scene_layer.SceneLayer;

/**
 * A solid color scene layer to use as a background for a layout without other composited content.
 */
@JNINamespace("android")
public class SolidColorSceneLayer extends SceneLayer {
    // NOTE: If you use SceneLayer's native pointer here, the JNI generator will try to
    // downcast using reinterpret_cast<>. We keep a separate pointer to avoid it.
    private long mNativePtr;

    /**
     * Set a background color for the scene layer.
     * @param backgroundColor The {@link ColorInt} for the color to set.
     */
    public void setBackgroundColor(@ColorInt int backgroundColor) {
        SolidColorSceneLayerJni.get().setBackgroundColor(mNativePtr, backgroundColor);
    }

    @Override
    protected void initializeNative() {
        if (mNativePtr == 0) {
            mNativePtr = SolidColorSceneLayerJni.get().init(SolidColorSceneLayer.this);
        }
        assert mNativePtr != 0;
    }

    @Override
    public void destroy() {
        super.destroy();
        mNativePtr = 0;
    }

    @NativeMethods
    @VisibleForTesting
    public interface Natives {
        long init(SolidColorSceneLayer caller);

        void setBackgroundColor(long nativeSolidColorSceneLayer, int backgroundColor);
    }
}
