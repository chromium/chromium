// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.continuous_search;

import android.graphics.RectF;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.layouts.EventFilter;
import org.chromium.chrome.browser.layouts.SceneOverlay;
import org.chromium.chrome.browser.layouts.components.VirtualView;
import org.chromium.chrome.browser.layouts.scene_layer.SceneLayer;
import org.chromium.chrome.browser.layouts.scene_layer.SceneOverlayLayer;
import org.chromium.ui.resources.ResourceManager;

import java.util.List;

/**
 * SceneLayer for Continuous Search Navigation used for compositing in CC.
 */
@JNINamespace("android")
class ContinuousSearchSceneLayer extends SceneOverlayLayer implements SceneOverlay {
    /** Handle to the native side of this class. */
    private long mNativePtr;

    /** The resource ID used to reference the view bitmap in native. */
    private int mResourceId;
    private int mVerticalOffset;
    private boolean mIsVisible;
    private final ResourceManager mResourceManager;

    public ContinuousSearchSceneLayer(ResourceManager resourceManager) {
        mResourceManager = resourceManager;
    }

    public void setVerticalOffset(int verticalOffset) {
        mVerticalOffset = verticalOffset;
    }

    public void setIsVisible(boolean visible) {
        mIsVisible = visible;
    }

    public void setResourceId(int id) {
        mResourceId = id;
    }

    @Override
    protected void initializeNative() {
        if (mNativePtr == 0) {
            mNativePtr = ContinuousSearchSceneLayerJni.get().init(ContinuousSearchSceneLayer.this);
        }
        assert mNativePtr != 0;
    }

    @Override
    public void setContentTree(SceneLayer contentTree) {
        ContinuousSearchSceneLayerJni.get().setContentTree(mNativePtr, contentTree);
    }

    @Override
    public SceneOverlayLayer getUpdatedSceneOverlayTree(
            RectF viewport, RectF visibleViewport, ResourceManager resourceManager, float yOffset) {
        ContinuousSearchSceneLayerJni.get().updateContinuousSearchLayer(
                mNativePtr, mResourceManager, mResourceId, mVerticalOffset);
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
    public void onSizeChanged(
            float width, float height, float visibleViewportOffsetY, int orientation) {}

    @Override
    public void getVirtualViews(List<VirtualView> views) {}

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

    @NativeMethods
    interface Natives {
        long init(ContinuousSearchSceneLayer caller);
        void setContentTree(long nativeContinuousSearchSceneLayer, SceneLayer contentTree);
        void updateContinuousSearchLayer(long nativeContinuousSearchSceneLayer,
                ResourceManager resourceManager, int viewResourceId, int offset);
    }
}
