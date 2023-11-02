// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.gesturenav;

import android.graphics.RectF;
import android.view.ViewGroup;

import org.chromium.chrome.browser.layouts.EventFilter;
import org.chromium.chrome.browser.layouts.SceneOverlay;
import org.chromium.chrome.browser.layouts.components.VirtualView;
import org.chromium.chrome.browser.layouts.scene_layer.SceneOverlayLayer;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.resources.ResourceManager;

import java.util.List;

/**
 * Handles overscroll glow effect when gesture navigation can't go forward any more.
 * Renders the effect on a compositor layer in scene overlay layer tree.
 */
class OverscrollGlowOverlay extends NavigationGlow implements SceneOverlay {
    private final OverscrollSceneLayer mSceneLayer;
    private final Runnable mRequestLayerUpdate;

    // True while the overlay is visible for showing overscroll effect.
    private boolean mIsShowing;

    private float mOffset;

    OverscrollGlowOverlay(WindowAndroid window, ViewGroup parentView, Runnable requestLayerUpdate) {
        super(parentView);
        mSceneLayer = new OverscrollSceneLayer(window, parentView);
        mRequestLayerUpdate = requestLayerUpdate;
    }

    // NavigationGlow implementation

    @Override
    public void prepare(float startX, float startY) {
        mSceneLayer.prepare(startX, startY);
        setIsShowing(true);
    }

    private void setIsShowing(boolean isShowing) {
        mIsShowing = isShowing;
        mOffset = 0.f;
    }

    @Override
    public void onScroll(float offset) {
        mOffset = offset;
        if (mIsShowing) mRequestLayerUpdate.run();
    }

    @Override
    public void release() {
        mSceneLayer.release();
        mOffset = 0.f;
    }

    @Override
    public void reset() {
        setIsShowing(false);
        mSceneLayer.reset();
    }

    @Override
    public void destroy() {
        mSceneLayer.destroy();
    }

    // SceneOverlay implementation

    @Override
    public SceneOverlayLayer getUpdatedSceneOverlayTree(
            RectF viewport, RectF visibleViewport, ResourceManager resourceManager, float yOffset) {
        if (!mSceneLayer.update(resourceManager, mOffset)) setIsShowing(false);
        return mSceneLayer;
    }

    @Override
    public boolean isSceneOverlayTreeShowing() {
        return mIsShowing;
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
        return true;
    }

    @Override
    public boolean onBackPressed() {
        return false;
    }

    @Override
    public boolean handlesTabCreating() {
        return false;
    }
}
