// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.status_indicator;

import android.graphics.RectF;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.layouts.SceneOverlay;
import org.chromium.chrome.browser.layouts.scene_layer.SceneLayer;
import org.chromium.chrome.browser.layouts.scene_layer.SceneOverlayLayer;
import org.chromium.ui.resources.ResourceManager;

/**
 * A composited view that is positioned below the status bar and is persistent. Typically used to
 * relay status, e.g. indicate user is offline.
 */
@JNINamespace("android")
@NullMarked
class StatusIndicatorSceneLayer extends SceneOverlayLayer implements SceneOverlay {
    /** Handle to the native side of this class. */
    private long mNativePtr;

    /** The resource ID used to reference the view bitmap in native. */
    private int mResourceId;

    /** The {@link BrowserControlsStateProvider} to access browser controls offsets. */
    private final BrowserControlsStateProvider mBrowserControlsStateProvider;

    private boolean mIsVisible;

    /**
     * Build a composited status view layer.
     * @param browserControlsStateProvider {@link BrowserControlsStateProvider} to access browser
     *                                     controls offsets.
     */
    StatusIndicatorSceneLayer(BrowserControlsStateProvider browserControlsStateProvider) {
        mBrowserControlsStateProvider = browserControlsStateProvider;
    }

    /**
     * Change the visibility of the scene layer.
     * @param visible True if visible.
     */
    public void setIsVisible(boolean visible) {
        mIsVisible = visible;
    }

    /**
     * Set the resource ID.
     * @param id Resource view ID.
     */
    public void setResourceId(int id) {
        mResourceId = id;
    }

    @Override
    protected void initializeNative() {
        if (mNativePtr == 0) {
            mNativePtr = StatusIndicatorSceneLayerJni.get().init(this);
        }
        assert mNativePtr != 0;
    }

    @Override
    public void setContentTree(SceneLayer contentTree) {
        StatusIndicatorSceneLayerJni.get().setContentTree(mNativePtr, contentTree);
    }

    @Override
    public SceneOverlayLayer getUpdatedSceneOverlayTree(
            RectF viewport, RectF visibleViewport, ResourceManager resourceManager) {
        final int offset = mBrowserControlsStateProvider.getTopControlsMinHeightOffset();
        StatusIndicatorSceneLayerJni.get()
                .updateStatusIndicatorLayer(mNativePtr, resourceManager, mResourceId, offset);

        return this;
    }

    @Override
    public boolean isSceneOverlayTreeShowing() {
        return mIsVisible;
    }

    @Override
    public void onSizeChanged(
            float width, float height, float visibleViewportOffsetY, int orientation) {}

    @NativeMethods
    interface Natives {
        long init(StatusIndicatorSceneLayer self);

        void setContentTree(long nativeStatusIndicatorSceneLayer, SceneLayer contentTree);

        void updateStatusIndicatorLayer(
                long nativeStatusIndicatorSceneLayer,
                ResourceManager resourceManager,
                int viewResourceId,
                int offset);
    }
}
