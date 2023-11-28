// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.scene_layer;

import android.content.Context;
import android.graphics.Color;

import androidx.annotation.ColorInt;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.chrome.browser.compositor.LayerTitleCache;
import org.chromium.chrome.browser.compositor.layouts.components.CompositorButton;
import org.chromium.chrome.browser.compositor.layouts.components.TintedCompositorButton;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutHelperManager;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutTab;
import org.chromium.chrome.browser.layouts.scene_layer.SceneLayer;
import org.chromium.chrome.browser.layouts.scene_layer.SceneOverlayLayer;
import org.chromium.ui.resources.ResourceManager;

/**
 * The Java component of what is basically a CC Layer that manages drawing the Tab Strip (which is
 * composed of {@link StripLayoutTab}s) to the screen.  This object keeps the layers up to date and
 * removes/creates children as necessary.  This object is built by its native counterpart.
 */
@JNINamespace("android")
public class TabStripSceneLayer extends SceneOverlayLayer {
    private static boolean sTestFlag;
    private long mNativePtr;
    private final float mDpToPx;

    public TabStripSceneLayer(Context context) {
        mDpToPx = context.getResources().getDisplayMetrics().density;
    }

    public static void setTestFlag(boolean testFlag) {
        sTestFlag = testFlag;
    }

    @Override
    protected void initializeNative() {
        if (mNativePtr == 0) {
            mNativePtr = TabStripSceneLayerJni.get().init(TabStripSceneLayer.this);
        }
        // Set flag for testing
        if (!sTestFlag) {
            assert mNativePtr != 0;
        }
    }

    @Override
    public void setContentTree(SceneLayer contentTree) {
        TabStripSceneLayerJni.get()
                .setContentTree(mNativePtr, TabStripSceneLayer.this, contentTree);
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
     * @param selectedTabId The ID of the selected tab.
     * @param hoveredTabId The ID of the hovered tab, if any. If no tab is hovered on, this ID will
     *         be invalid.
     */
    public void pushAndUpdateStrip(
            StripLayoutHelperManager layoutHelper,
            LayerTitleCache layerTitleCache,
            ResourceManager resourceManager,
            StripLayoutTab[] stripLayoutTabsToRender,
            float yOffset,
            int selectedTabId,
            int hoveredTabId) {
        if (mNativePtr == 0) return;
        final boolean visible = yOffset > -layoutHelper.getHeight();
        // This will hide the tab strips if necessary.
        TabStripSceneLayerJni.get()
                .beginBuildingFrame(mNativePtr, TabStripSceneLayer.this, visible);
        // When strip tabs are completely off screen, we don't need to update it.
        if (visible) {
            pushButtonsAndBackground(layoutHelper, resourceManager, yOffset);
            pushStripTabs(
                    layoutHelper,
                    layerTitleCache,
                    resourceManager,
                    stripLayoutTabsToRender,
                    selectedTabId,
                    hoveredTabId);
        }
        TabStripSceneLayerJni.get().finishBuildingFrame(mNativePtr, TabStripSceneLayer.this);
    }

    private void pushButtonsAndBackground(
            StripLayoutHelperManager layoutHelper, ResourceManager resourceManager, float yOffset) {
        final int width = Math.round(layoutHelper.getWidth() * mDpToPx);
        final int height = Math.round(layoutHelper.getHeight() * mDpToPx);
        TabStripSceneLayerJni.get()
                .updateTabStripLayer(
                        mNativePtr,
                        TabStripSceneLayer.this,
                        width,
                        height,
                        yOffset * mDpToPx,
                        layoutHelper.getBackgroundColor());

        TintedCompositorButton newTabButton = layoutHelper.getNewTabButton();
        CompositorButton modelSelectorButton = layoutHelper.getModelSelectorButton();
        boolean modelSelectorButtonVisible = modelSelectorButton.isVisible();
        boolean newTabButtonVisible = newTabButton.isVisible();
        TabStripSceneLayerJni.get()
                .updateNewTabButton(
                        mNativePtr,
                        TabStripSceneLayer.this,
                        newTabButton.getResourceId(),
                        newTabButton.getBackgroundResourceId(),
                        newTabButton.getShouldApplyHoverBackground(),
                        newTabButton.getX() * mDpToPx,
                        newTabButton.getY() * mDpToPx,
                        layoutHelper.getNewTabBtnVisualOffset() * mDpToPx,
                        newTabButtonVisible,
                        newTabButton.getTint(),
                        newTabButton.getBackgroundTint(),
                        newTabButton.getOpacity(),
                        resourceManager);
        TabStripSceneLayerJni.get()
                .updateModelSelectorButtonBackground(
                        mNativePtr,
                        TabStripSceneLayer.this,
                        modelSelectorButton.getResourceId(),
                        ((TintedCompositorButton) modelSelectorButton).getBackgroundResourceId(),
                        modelSelectorButton.getX() * mDpToPx,
                        modelSelectorButton.getY() * mDpToPx,
                        modelSelectorButton.getWidth() * mDpToPx,
                        modelSelectorButton.getHeight() * mDpToPx,
                        modelSelectorButton.isIncognito(),
                        modelSelectorButtonVisible,
                        ((TintedCompositorButton) modelSelectorButton).getTint(),
                        ((TintedCompositorButton) modelSelectorButton).getBackgroundTint(),
                        ((TintedCompositorButton) modelSelectorButton)
                                .getShouldApplyHoverBackground(),
                        modelSelectorButton.getOpacity(),
                        resourceManager);

        TabStripSceneLayerJni.get()
                .updateTabStripLeftFade(
                        mNativePtr,
                        TabStripSceneLayer.this,
                        layoutHelper.getLeftFadeDrawable(),
                        layoutHelper.getLeftFadeOpacity(),
                        resourceManager,
                        layoutHelper.getBackgroundColor());

        TabStripSceneLayerJni.get()
                .updateTabStripRightFade(
                        mNativePtr,
                        TabStripSceneLayer.this,
                        layoutHelper.getRightFadeDrawable(),
                        layoutHelper.getRightFadeOpacity(),
                        resourceManager,
                        layoutHelper.getBackgroundColor());
    }

    private void pushStripTabs(
            StripLayoutHelperManager layoutHelper,
            LayerTitleCache layerTitleCache,
            ResourceManager resourceManager,
            StripLayoutTab[] stripTabs,
            int selectedTabId,
            int hoveredTabId) {
        final int tabsCount = stripTabs != null ? stripTabs.length : 0;

        // TODO(https://crbug.com/1450380): Cleanup params, as some don't change and others are now
        //  unused.
        for (int i = 0; i < tabsCount; i++) {
            final StripLayoutTab st = stripTabs[i];
            boolean isSelected = st.getId() == selectedTabId;
            boolean isHovered = st.getId() == hoveredTabId;

            TabStripSceneLayerJni.get()
                    .putStripTabLayer(
                            mNativePtr,
                            TabStripSceneLayer.this,
                            st.getId(),
                            st.getCloseButton().getResourceId(),
                            st.getCloseButton().getBackgroundResourceId(),
                            st.getDividerResourceId(),
                            st.getResourceId(),
                            st.getOutlineResourceId(),
                            st.getCloseButton().getTint(),
                            st.getCloseButton().getBackgroundTint(),
                            st.getDividerTint(),
                            st.getTint(isSelected, isHovered),
                            Color.TRANSPARENT,
                            isSelected,
                            st.getClosePressed(),
                            layoutHelper.getWidth() * mDpToPx,
                            st.getDrawX() * mDpToPx,
                            st.getDrawY() * mDpToPx,
                            st.getWidth() * mDpToPx,
                            st.getHeight() * mDpToPx,
                            st.getContentOffsetY() * mDpToPx,
                            st.getDividerOffsetX() * mDpToPx,
                            st.getBottomMargin() * mDpToPx,
                            st.getTopMargin() * mDpToPx,
                            st.getCloseButtonPadding() * mDpToPx,
                            st.getCloseButton().getOpacity(),
                            st.isStartDividerVisible(),
                            st.isEndDividerVisible(),
                            st.isLoading(),
                            st.getLoadingSpinnerRotation(),
                            st.getBrightness(),
                            st.getContainerOpacity(),
                            layerTitleCache,
                            resourceManager);
        }
    }

    @Override
    public void destroy() {
        super.destroy();
        mNativePtr = 0;
    }

    @NativeMethods
    public interface Natives {
        long init(TabStripSceneLayer caller);

        void beginBuildingFrame(
                long nativeTabStripSceneLayer, TabStripSceneLayer caller, boolean visible);

        void finishBuildingFrame(long nativeTabStripSceneLayer, TabStripSceneLayer caller);

        void updateTabStripLayer(
                long nativeTabStripSceneLayer,
                TabStripSceneLayer caller,
                int width,
                int height,
                float yOffset,
                @ColorInt int backgroundColor);

        void updateNewTabButton(
                long nativeTabStripSceneLayer,
                TabStripSceneLayer caller,
                int resourceId,
                int backgroundResourceId,
                boolean isHovered,
                float x,
                float y,
                float touchTargetOffset,
                boolean visible,
                int tint,
                int backgroundTint,
                float buttonAlpha,
                ResourceManager resourceManager);

        void updateModelSelectorButton(
                long nativeTabStripSceneLayer,
                TabStripSceneLayer caller,
                int resourceId,
                float x,
                float y,
                float width,
                float height,
                boolean incognito,
                boolean visible,
                float buttonAlpha,
                ResourceManager resourceManager);

        void updateModelSelectorButtonBackground(
                long nativeTabStripSceneLayer,
                TabStripSceneLayer caller,
                int resourceId,
                int backgroundResourceId,
                float x,
                float y,
                float width,
                float height,
                boolean incognito,
                boolean visible,
                int tint,
                int backgroundTint,
                boolean isHovered,
                float buttonAlpha,
                ResourceManager resourceManager);

        void updateTabStripLeftFade(
                long nativeTabStripSceneLayer,
                TabStripSceneLayer caller,
                int resourceId,
                float opacity,
                ResourceManager resourceManager,
                @ColorInt int leftFadeColor);

        void updateTabStripRightFade(
                long nativeTabStripSceneLayer,
                TabStripSceneLayer caller,
                int resourceId,
                float opacity,
                ResourceManager resourceManager,
                @ColorInt int rightFadeColor);

        void putStripTabLayer(
                long nativeTabStripSceneLayer,
                TabStripSceneLayer caller,
                int id,
                int closeResourceId,
                int closeBackgroundResourceId,
                int dividerResourceId,
                int handleResourceId,
                int handleOutlineResourceId,
                int closeTint,
                int closeHoverBackgroundTint,
                int dividerTint,
                int handleTint,
                int handleOutlineTint,
                boolean foreground,
                boolean closePressed,
                float toolbarWidth,
                float x,
                float y,
                float width,
                float height,
                float contentOffsetY,
                float dividerOffsetX,
                float bottomMargin,
                float topMargin,
                float closeButtonPadding,
                float closeButtonAlpha,
                boolean isStartDividerVisible,
                boolean isEndDividerVisible,
                boolean isLoading,
                float spinnerRotation,
                float brightness,
                float opacity,
                LayerTitleCache layerTitleCache,
                ResourceManager resourceManager);

        void setContentTree(
                long nativeTabStripSceneLayer, TabStripSceneLayer caller, SceneLayer contentTree);
    }

    public void initializeNativeForTesting() {
        this.initializeNative();
    }
}
