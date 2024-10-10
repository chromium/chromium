// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud;

import android.graphics.RectF;

import androidx.annotation.ColorInt;
import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.layouts.EventFilter;
import org.chromium.chrome.browser.layouts.SceneOverlay;
import org.chromium.chrome.browser.layouts.components.VirtualView;
import org.chromium.chrome.browser.layouts.scene_layer.SceneLayer;
import org.chromium.chrome.browser.layouts.scene_layer.SceneOverlayLayer;
import org.chromium.ui.resources.ResourceManager;

import java.util.List;

/**
 * A composited view that is positioned below the bottom controls container and is persistent. It is
 * a solid-color rectangle that acts as a placeholder for the mini player UI while transitioning
 * between hidden and shown states.
 */
@JNINamespace("android")
public class ReadAloudMiniPlayerSceneLayer extends SceneOverlayLayer implements SceneOverlay {
    /** Handle to the native side of this class. */
    private long mNativePtr;

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
            @NonNull BrowserControlsStateProvider browserControlsStateProvider) {
        mBrowserControlsStateProvider = browserControlsStateProvider;
    }

    @Override
    public void destroy() {
        if (mNativePtr != 0L) {
            ReadAloudMiniPlayerSceneLayerJni.get().destroy(mNativePtr, this);
            mNativePtr = 0L;
        }
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
        if (mNativePtr == 0) {
            mNativePtr =
                    ReadAloudMiniPlayerSceneLayerJni.get().init(ReadAloudMiniPlayerSceneLayer.this);
        }
        assert mNativePtr != 0;
    }

    @Override
    public void setContentTree(SceneLayer contentTree) {
        ReadAloudMiniPlayerSceneLayerJni.get().setContentTree(mNativePtr, contentTree);
    }

    @Override
    public SceneOverlayLayer getUpdatedSceneOverlayTree(
            RectF viewport,
            RectF visibleViewport,
            ResourceManager resourceManager,
            float topOffset) {
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
        return mIsVisible;
    }

    @Override
    public EventFilter getEventFilter() {
        return null;
    }

    @Override
    public boolean shouldHideAndroidBrowserControls() {
        return false;
    }

    @Override
    public boolean updateOverlay(long time, long dt) {
        return false;
    }

    @Override
    public boolean onBackPressed() {
        return false;
    }

    @Override
    public boolean handlesTabCreating() {
        return false;
    }

    @Override
    public void onSizeChanged(
            float width, float height, float visibleViewportOffsetY, int orientation) {}

    @Override
    public void getVirtualViews(List<VirtualView> views) {}

    @VisibleForTesting
    @NativeMethods
    public interface Natives {
        long init(ReadAloudMiniPlayerSceneLayer caller);

        void destroy(
                long nativeReadAloudMiniPlayerSceneLayer, ReadAloudMiniPlayerSceneLayer caller);

        void setContentTree(long nativeReadAloudMiniPlayerSceneLayer, SceneLayer contentTree);

        void updateReadAloudMiniPlayerLayer(
                long nativeReadAloudMiniPlayerSceneLayer,
                int colorArgb,
                int width,
                int viewportHeight,
                int containerHeight,
                int bottomOffset);
    }
}
