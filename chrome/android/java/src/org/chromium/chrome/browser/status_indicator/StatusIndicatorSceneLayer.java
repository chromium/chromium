// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.status_indicator;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.graphics.RectF;

import androidx.annotation.Px;

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
 * A composited view that is positioned below the status bar and is persistent. Typically used to
 * relay status, e.g. indicate user is offline.
 */
@JNINamespace("android")
@NullMarked
class StatusIndicatorSceneLayer extends SceneOverlayLayer implements SceneOverlay {
    @JniType("::android::StatusIndicatorSceneLayer")
    interface NativeStatusIndicatorSceneLayer extends JniTypeToken {}

    /** Handle to the native side of this class. */
    private @Nullable JniRawPtr<NativeStatusIndicatorSceneLayer> mNativePtr;

    /** The resource ID used to reference the view bitmap in native. */
    private int mResourceId;

    /** The {@link BrowserControlsStateProvider} to access browser controls offsets. */
    private final BrowserControlsStateProvider mBrowserControlsStateProvider;

    private boolean mIsVisible;
    private @Px int mTopOffset;

    /**
     * Build a composited status view layer.
     *
     * @param browserControlsStateProvider {@link BrowserControlsStateProvider} to access browser
     *     controls offsets.
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
     * Set the top offset of the scene layer.
     *
     * @param topOffset Top offset in pixels.
     */
    public void setTopOffset(@Px int topOffset) {
        mTopOffset = topOffset;
    }

    /**
     * Set the resource ID.
     *
     * @param id Resource view ID.
     */
    public void setResourceId(int id) {
        mResourceId = id;
    }

    @Override
    protected void initializeNative() {
        if (mNativePtr == null) {
            mNativePtr = StatusIndicatorSceneLayerJni.get().init(this);
        }
        assert mNativePtr != null;
    }

    @Override
    public void destroy() {
        if (mNativePtr == null) return;
        mNativePtr.release();
        mNativePtr = null;
        super.destroy();
    }

    @Override
    public void setContentTree(SceneLayer contentTree) {
        StatusIndicatorSceneLayerJni.get().setContentTree(assumeNonNull(mNativePtr), contentTree);
    }

    @Override
    public SceneOverlayLayer getUpdatedSceneOverlayTree(
            RectF viewport, RectF visibleViewport, ResourceManager resourceManager) {
        final int visibleHeight =
                Math.max(
                        0,
                        mBrowserControlsStateProvider.getTopControlsMinHeightOffset() - mTopOffset);
        StatusIndicatorSceneLayerJni.get()
                .updateStatusIndicatorLayer(
                        assumeNonNull(mNativePtr),
                        resourceManager,
                        mResourceId,
                        mTopOffset,
                        visibleHeight);

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
        JniRawPtr<NativeStatusIndicatorSceneLayer> init(StatusIndicatorSceneLayer self);

        void setContentTree(JniPtr<NativeStatusIndicatorSceneLayer> self, SceneLayer contentTree);

        void updateStatusIndicatorLayer(
                JniPtr<NativeStatusIndicatorSceneLayer> self,
                ResourceManager resourceManager,
                int viewResourceId,
                int topOffset,
                int visibleHeight);
    }
}
