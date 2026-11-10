// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud;

import android.graphics.RectF;

import androidx.annotation.ColorInt;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.JNINamespace;
import org.jni_zero.JniPtr;
import org.jni_zero.JniRawPtr;
import org.jni_zero.JniType;
import org.jni_zero.JniTypeToken;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.layouts.SceneOverlay;
import org.chromium.chrome.browser.layouts.scene_layer.SceneLayer;
import org.chromium.chrome.browser.layouts.scene_layer.SceneOverlayLayer;
import org.chromium.ui.resources.ResourceManager;

/**
 * A composited view that is positioned below the bottom controls container and is persistent. It is
 * a solid-color rectangle that acts as a placeholder for the mini player UI while transitioning
 * between hidden and shown states.
 */
@NullMarked
@JNINamespace("android")
public class ReadAloudMiniPlayerSceneLayer extends SceneOverlayLayer implements SceneOverlay {
    @JniType("::android::ReadAloudMiniPlayerSceneLayer")
    interface NativeReadAloudMiniPlayerSceneLayer extends JniTypeToken {}

    /** Handle to the native side of this class. */
    private @Nullable JniRawPtr<NativeReadAloudMiniPlayerSceneLayer> mNativePtr;

    /** The {@link BrowserControlsStateProvider} to access browser controls offsets. */
    private final BrowserControlsStateProvider mBrowserControlsStateProvider;

    private boolean mIsVisible;
    private int mHeight;
    private @ColorInt int mColorArgb;

    /**
     * Build the composited mini player placeholder.
     *
     * @param browserControlsStateProvider {@link BrowserControlsStateProvider} to access browser
     *     controls offsets.
     */
    public ReadAloudMiniPlayerSceneLayer(
            BrowserControlsStateProvider browserControlsStateProvider) {
        mBrowserControlsStateProvider = browserControlsStateProvider;
    }

    @Override
    public void destroy() {
        mIsVisible = false;
        if (mNativePtr == null) return;
        mNativePtr.release();
        mNativePtr = null;
        super.destroy();
    }

    /**
     * Set the view size.
     *
     * @param width Width in pixels.
     * @param height Height in pixels.
     */
    public void setSize(int width, int height) {
        mHeight = height;
    }

    /**
     * Set the view background color.
     *
     * @param colorArgb Color in ARGB 8888 format.
     */
    public void setColor(@ColorInt int colorArgb) {
        mColorArgb = colorArgb;
    }

    /**
     * Change the visibility of the scene layer.
     *
     * @param visible True if visible.
     */
    public void setIsVisible(boolean visible) {
        mIsVisible = visible;
    }

    @Override
    protected void initializeNative() {
        if (mNativePtr == null) {
            mNativePtr = ReadAloudMiniPlayerSceneLayerJni.get().init(this);
        }
        assert mNativePtr != null;
    }

    @Override
    public void setContentTree(SceneLayer contentTree) {
        if (mNativePtr == null) return;
        ReadAloudMiniPlayerSceneLayerJni.get().setContentTree(mNativePtr, contentTree);
    }

    @Override
    public @Nullable SceneOverlayLayer getUpdatedSceneOverlayTree(
            RectF viewport, RectF visibleViewport, ResourceManager resourceManager) {
        if (mNativePtr == null) return null;
        ReadAloudMiniPlayerSceneLayerJni.get()
                .updateReadAloudMiniPlayerLayer(
                        mNativePtr,
                        mColorArgb,
                        /* width= */ (int) viewport.width(),
                        /* viewportHeight= */ (int) viewport.height(),
                        /* containerHeight= */ mHeight,
                        /* bottomOffset= */ mBrowserControlsStateProvider
                                .getBottomControlsMinHeightOffset());
        return this;
    }

    @Override
    public boolean isSceneOverlayTreeShowing() {
        return mIsVisible && mNativePtr != null;
    }

    @Override
    public void onSizeChanged(
            float width, float height, float visibleViewportOffsetY, int orientation) {}

    @VisibleForTesting
    @NativeMethods
    public interface Natives {
        JniRawPtr<NativeReadAloudMiniPlayerSceneLayer> init(ReadAloudMiniPlayerSceneLayer self);

        void setContentTree(
                JniPtr<NativeReadAloudMiniPlayerSceneLayer> self, SceneLayer contentTree);

        void updateReadAloudMiniPlayerLayer(
                JniPtr<NativeReadAloudMiniPlayerSceneLayer> self,
                int colorArgb,
                int width,
                int viewportHeight,
                int containerHeight,
                int bottomOffset);
    }
}
