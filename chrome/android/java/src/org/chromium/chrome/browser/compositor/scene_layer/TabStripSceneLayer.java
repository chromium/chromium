// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.scene_layer;

import android.content.Context;
import android.os.Build;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.LayerTitleCache;
import org.chromium.chrome.browser.compositor.layouts.components.CompositorButton;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutHelperManager;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutTab;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.resources.ResourceManager;

/**
 * The Java component of what is basically a CC Layer that manages drawing the Tab Strip (which is
 * composed of {@link StripLayoutTab}s) to the screen.  This object keeps the layers up to date and
 * removes/creates children as necessary.  This object is built by its native counterpart.
 */
@JNINamespace("android")
public class TabStripSceneLayer extends SceneOverlayLayer {
    private long mNativePtr;
    private final float mDpToPx;
    private SceneLayer mChildSceneLayer;
    private int mOrientation;
    private int mNumReaddBackground;

    public TabStripSceneLayer(Context context) {
        mDpToPx = context.getResources().getDisplayMetrics().density;
    }

    @Override
    protected void initializeNative() {
        if (mNativePtr == 0) {
            mNativePtr = TabStripSceneLayerJni.get().init(TabStripSceneLayer.this);
        }
        assert mNativePtr != 0;
    }

    @Override
    public void setContentTree(SceneLayer contentTree) {
        TabStripSceneLayerJni.get().setContentTree(
                mNativePtr, TabStripSceneLayer.this, contentTree);
    }

    /**
     * Pushes all relevant {@link StripLayoutTab}s to the CC Layer tree.
     * This also pushes any other assets required to draw the Tab Strip.  This should only be called
     * when the Compositor has disabled ScheduleComposite calls as this will change the tree and
     * could subsequently cause unnecessary follow up renders.
     *
     * @param layoutHelper A layout helper for the tab strip.
     * @param layerTitleCache A layer title cache.
     * @param resourceManager A resource manager.
     * @param stripLayoutTabsToRender Array of strip layout tabs.
     * @param yOffset Current browser controls offset in dp.
     */
    public void pushAndUpdateStrip(StripLayoutHelperManager layoutHelper,
            LayerTitleCache layerTitleCache, ResourceManager resourceManager,
            StripLayoutTab[] stripLayoutTabsToRender, float yOffset, int selectedTabId) {
        if (mNativePtr == 0) return;

        final boolean visible = yOffset > -layoutHelper.getHeight();
        // This will hide the tab strips if necessary.
        TabStripSceneLayerJni.get().beginBuildingFrame(
                mNativePtr, TabStripSceneLayer.this, visible);
        // When strip tabs are completely off screen, we don't need to update it.
        if (visible) {
            pushButtonsAndBackground(layoutHelper, resourceManager, yOffset);
            pushStripTabs(layoutHelper, layerTitleCache, resourceManager, stripLayoutTabsToRender,
                    selectedTabId);
        }
        TabStripSceneLayerJni.get().finishBuildingFrame(mNativePtr, TabStripSceneLayer.this);
    }

    private boolean shouldReaddBackground(int orientation) {
        // Sometimes layer trees do not get updated on rotation on Nexus 10.
        // This is a workaround that readds the background to prevent it.
        // See https://crbug.com/503930 for more.
        if (Build.MODEL == null || !Build.MODEL.contains("Nexus 10")) return false;
        if (mOrientation != orientation) {
            // This is a random number. Empirically this is enough.
            mNumReaddBackground = 10;
            mOrientation = orientation;
        }
        mNumReaddBackground--;
        return mNumReaddBackground >= 0;
    }

    private void pushButtonsAndBackground(StripLayoutHelperManager layoutHelper,
            ResourceManager resourceManager, float yOffset) {
        final float width = layoutHelper.getWidth() * mDpToPx;
        final float height = layoutHelper.getHeight() * mDpToPx;
        TabStripSceneLayerJni.get().updateTabStripLayer(mNativePtr, TabStripSceneLayer.this, width,
                height, yOffset * mDpToPx, layoutHelper.getBackgroundTabBrightness(),
                layoutHelper.getBrightness(), shouldReaddBackground(layoutHelper.getOrientation()));

        CompositorButton newTabButton = layoutHelper.getNewTabButton();
        CompositorButton modelSelectorButton = layoutHelper.getModelSelectorButton();
        boolean newTabButtonVisible = newTabButton.isVisible();
        boolean modelSelectorButtonVisible = modelSelectorButton.isVisible();

        TabStripSceneLayerJni.get().updateNewTabButton(mNativePtr, TabStripSceneLayer.this,
                newTabButton.getResourceId(), newTabButton.getX() * mDpToPx,
                newTabButton.getY() * mDpToPx, newTabButton.getWidth() * mDpToPx,
                newTabButton.getHeight() * mDpToPx, newTabButtonVisible, resourceManager);

        TabStripSceneLayerJni.get().updateModelSelectorButton(mNativePtr, TabStripSceneLayer.this,
                modelSelectorButton.getResourceId(), modelSelectorButton.getX() * mDpToPx,
                modelSelectorButton.getY() * mDpToPx, modelSelectorButton.getWidth() * mDpToPx,
                modelSelectorButton.getHeight() * mDpToPx, modelSelectorButton.isIncognito(),
                modelSelectorButtonVisible, resourceManager);

        int leftFadeDrawable = modelSelectorButtonVisible && LocalizationUtils.isLayoutRtl()
                ? R.drawable.tab_strip_fade_for_model_selector : R.drawable.tab_strip_fade;
        int rightFadeDrawable = modelSelectorButtonVisible && !LocalizationUtils.isLayoutRtl()
                ? R.drawable.tab_strip_fade_for_model_selector : R.drawable.tab_strip_fade;

        TabStripSceneLayerJni.get().updateTabStripLeftFade(mNativePtr, TabStripSceneLayer.this,
                leftFadeDrawable, layoutHelper.getLeftFadeOpacity(), resourceManager);

        TabStripSceneLayerJni.get().updateTabStripRightFade(mNativePtr, TabStripSceneLayer.this,
                rightFadeDrawable, layoutHelper.getRightFadeOpacity(), resourceManager);
    }

    private void pushStripTabs(StripLayoutHelperManager layoutHelper,
            LayerTitleCache layerTitleCache, ResourceManager resourceManager,
            StripLayoutTab[] stripTabs, int selectedTabId) {
        final int tabsCount = stripTabs != null ? stripTabs.length : 0;

        for (int i = 0; i < tabsCount; i++) {
            final StripLayoutTab st = stripTabs[i];
            boolean isSelected = st.getId() == selectedTabId;
            TabStripSceneLayerJni.get().putStripTabLayer(mNativePtr, TabStripSceneLayer.this,
                    st.getId(), st.getCloseButton().getResourceId(), st.getResourceId(),
                    st.getOutlineResourceId(), st.getCloseButton().getTint(),
                    st.getTint(isSelected), st.getOutlineTint(isSelected), isSelected,
                    st.getClosePressed(), layoutHelper.getWidth() * mDpToPx,
                    st.getDrawX() * mDpToPx, st.getDrawY() * mDpToPx, st.getWidth() * mDpToPx,
                    st.getHeight() * mDpToPx, st.getContentOffsetX() * mDpToPx,
                    st.getCloseButton().getOpacity(), st.isLoading(),
                    st.getLoadingSpinnerRotation(), layerTitleCache, resourceManager);
        }
    }

    @Override
    public void destroy() {
        super.destroy();
        mNativePtr = 0;
    }

    @NativeMethods
    interface Natives {
        long init(TabStripSceneLayer caller);
        void beginBuildingFrame(
                long nativeTabStripSceneLayer, TabStripSceneLayer caller, boolean visible);
        void finishBuildingFrame(long nativeTabStripSceneLayer, TabStripSceneLayer caller);
        void updateTabStripLayer(long nativeTabStripSceneLayer, TabStripSceneLayer caller,
                float width, float height, float yOffset, float backgroundTabBrightness,
                float brightness, boolean shouldReaddBackground);
        void updateNewTabButton(long nativeTabStripSceneLayer, TabStripSceneLayer caller,
                int resourceId, float x, float y, float width, float height, boolean visible,
                ResourceManager resourceManager);
        void updateModelSelectorButton(long nativeTabStripSceneLayer, TabStripSceneLayer caller,
                int resourceId, float x, float y, float width, float height, boolean incognito,
                boolean visible, ResourceManager resourceManager);
        void updateTabStripLeftFade(long nativeTabStripSceneLayer, TabStripSceneLayer caller,
                int resourceId, float opacity, ResourceManager resourceManager);
        void updateTabStripRightFade(long nativeTabStripSceneLayer, TabStripSceneLayer caller,
                int resourceId, float opacity, ResourceManager resourceManager);
        void putStripTabLayer(long nativeTabStripSceneLayer, TabStripSceneLayer caller, int id,
                int closeResourceId, int handleResourceId, int handleOutlineResourceId,
                int closeTint, int handleTint, int handleOutlineTint, boolean foreground,
                boolean closePressed, float toolbarWidth, float x, float y, float width,
                float height, float contentOffsetX, float closeButtonAlpha, boolean isLoading,
                float spinnerRotation, LayerTitleCache layerTitleCache,
                ResourceManager resourceManager);
        void setContentTree(
                long nativeTabStripSceneLayer, TabStripSceneLayer caller, SceneLayer contentTree);
    }
}
