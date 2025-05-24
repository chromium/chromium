// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.scene_layer;

import static org.chromium.chrome.browser.tasks.tab_management.TabUiThemeUtil.FOLIO_FOOT_LENGTH_DP;

import androidx.annotation.ColorInt;
import androidx.annotation.DrawableRes;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.Token;
import org.chromium.cc.input.OffsetTag;
import org.chromium.chrome.browser.compositor.LayerTitleCache;
import org.chromium.chrome.browser.compositor.layouts.components.CompositorButton;
import org.chromium.chrome.browser.compositor.layouts.components.TintedCompositorButton;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutGroupTitle;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutHelperManager;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutTab;
import org.chromium.chrome.browser.layouts.scene_layer.SceneLayer;
import org.chromium.chrome.browser.layouts.scene_layer.SceneOverlayLayer;
import org.chromium.chrome.browser.tasks.tab_management.TabUiThemeUtil;
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

    /**
     * @param density Density for Dp to Px conversion.
     */
    public TabStripSceneLayer(float density) {
        mDpToPx = density;
        TabStripSceneLayerJni.get()
                .setConstants(
                        mNativePtr,
                        Math.round(StripLayoutGroupTitle.REORDER_BACKGROUND_TOP_MARGIN * mDpToPx),
                        Math.round(
                                StripLayoutGroupTitle.REORDER_BACKGROUND_BOTTOM_MARGIN * mDpToPx),
                        Math.round(
                                StripLayoutGroupTitle.REORDER_BACKGROUND_PADDING_START * mDpToPx),
                        Math.round(StripLayoutGroupTitle.REORDER_BACKGROUND_PADDING_END * mDpToPx),
                        Math.round(
                                StripLayoutGroupTitle.REORDER_BACKGROUND_CORNER_RADIUS * mDpToPx));
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
     * Pushes all relevant {@link StripLayoutTab}s to the CC Layer tree. This also pushes any other
     * assets required to draw the Tab Strip. This should only be called when the Compositor has
     * disabled ScheduleComposite calls as this will change the tree and could subsequently cause
     * unnecessary follow up renders.
     *
     * @param layoutHelper A layout helper for the tab strip.
     * @param layerTitleCache A layer title cache.
     * @param resourceManager A resource manager.
     * @param stripLayoutTabsToRender Array of strip layout tabs.
     * @param yOffset Current browser controls offset in dp.
     * @param selectedTabId The ID of the selected tab.
     * @param hoveredTabId The ID of the hovered tab, if any. If no tab is hovered on, this ID will
     *     be invalid.
     * @param scrimColor The color of the scrim overlay that covers the tab strip.
     * @param scrimOpacity The opacity of the scrim overlay that covers the tab strip.
     */
    public void pushAndUpdateStrip(
            StripLayoutHelperManager layoutHelper,
            LayerTitleCache layerTitleCache,
            ResourceManager resourceManager,
            StripLayoutTab[] stripLayoutTabsToRender,
            StripLayoutGroupTitle[] stripLayoutGroupTitlesToRender,
            float yOffset,
            int selectedTabId,
            int hoveredTabId,
            int scrimColor,
            float scrimOpacity,
            float leftPaddingDp,
            float rightPaddingDp,
            float topPaddingDp) {

        if (mNativePtr == 0) return;
        final boolean visible = yOffset > -layoutHelper.getHeight();

        // This will hide the tab strips if necessary.
        TabStripSceneLayerJni.get()
                .beginBuildingFrame(mNativePtr, TabStripSceneLayer.this, visible);
        // When strip tabs are completely off screen, we don't need to update it.
        if (visible) {
            // Ceil the padding to avoid off-by-one issues similar to crbug/329722454. This is
            // required since these values are originated from Android UI.
            float leftPaddingPx = (float) Math.ceil(leftPaddingDp * mDpToPx);
            float rightPaddingPx = (float) Math.ceil(rightPaddingDp * mDpToPx);
            float topPaddingPx = (float) Math.ceil(topPaddingDp * mDpToPx);

            pushButtonsAndBackground(
                    layoutHelper,
                    resourceManager,
                    yOffset,
                    scrimColor,
                    scrimOpacity,
                    leftPaddingPx,
                    rightPaddingPx,
                    topPaddingPx);
            pushGroupIndicators(stripLayoutGroupTitlesToRender, layerTitleCache, resourceManager);
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

    public void updateOffsetTag(OffsetTag offsetTag) {
        TabStripSceneLayerJni.get().updateOffsetTag(mNativePtr, TabStripSceneLayer.this, offsetTag);
    }

    @VisibleForTesting
    /* package */ void pushButtonsAndBackground(
            StripLayoutHelperManager layoutHelper,
            ResourceManager resourceManager,
            float yOffset,
            @ColorInt int scrimColor,
            float scrimOpacity,
            float leftPaddingPx,
            float rightPaddingPx,
            float topPaddingPx) {
        final int width = Math.round(layoutHelper.getWidth() * mDpToPx);
        final int height = Math.round(layoutHelper.getHeight() * mDpToPx);
        TabStripSceneLayerJni.get()
                .updateTabStripLayer(
                        mNativePtr,
                        TabStripSceneLayer.this,
                        width,
                        height,
                        yOffset * mDpToPx,
                        layoutHelper.getBackgroundColor(),
                        scrimColor,
                        scrimOpacity,
                        leftPaddingPx,
                        rightPaddingPx,
                        topPaddingPx);

        TintedCompositorButton newTabButton = layoutHelper.getNewTabButton();
        boolean newTabButtonVisible = newTabButton.isVisible();
        TabStripSceneLayerJni.get()
                .updateNewTabButton(
                        mNativePtr,
                        TabStripSceneLayer.this,
                        newTabButton.getResourceId(),
                        newTabButton.getBackgroundResourceId(),
                        newTabButton.getDrawX() * mDpToPx,
                        newTabButton.getDrawY() * mDpToPx,
                        layoutHelper.getNewTabBtnVisualOffset() * mDpToPx,
                        newTabButtonVisible,
                        newTabButton.getShouldApplyHoverBackground(),
                        newTabButton.getTint(),
                        newTabButton.getBackgroundTint(),
                        newTabButton.getOpacity(),
                        newTabButton.isKeyboardFocused(),
                        TabUiThemeUtil.getCircularButtonKeyboardFocusDrawableRes(),
                        newTabButton.getKeyboardFocusRingColor(),
                        resourceManager);

        CompositorButton modelSelectorButton = layoutHelper.getModelSelectorButton();
        if (modelSelectorButton != null) {
            boolean modelSelectorButtonVisible = modelSelectorButton.isVisible();
            TabStripSceneLayerJni.get()
                    .updateModelSelectorButton(
                            mNativePtr,
                            TabStripSceneLayer.this,
                            modelSelectorButton.getResourceId(),
                            ((TintedCompositorButton) modelSelectorButton)
                                    .getBackgroundResourceId(),
                            modelSelectorButton.getDrawX() * mDpToPx,
                            modelSelectorButton.getDrawY() * mDpToPx,
                            modelSelectorButtonVisible,
                            modelSelectorButton.getShouldApplyHoverBackground(),
                            ((TintedCompositorButton) modelSelectorButton).getTint(),
                            ((TintedCompositorButton) modelSelectorButton).getBackgroundTint(),
                            modelSelectorButton.getOpacity(),
                            modelSelectorButton.isKeyboardFocused(),
                            TabUiThemeUtil.getCircularButtonKeyboardFocusDrawableRes(),
                            modelSelectorButton.getKeyboardFocusRingColor(),
                            resourceManager);
        }

        TabStripSceneLayerJni.get()
                .updateTabStripLeftFade(
                        mNativePtr,
                        TabStripSceneLayer.this,
                        layoutHelper.getLeftFadeDrawable(),
                        layoutHelper.getLeftFadeOpacity(),
                        resourceManager,
                        layoutHelper.getBackgroundColor(),
                        leftPaddingPx);

        TabStripSceneLayerJni.get()
                .updateTabStripRightFade(
                        mNativePtr,
                        TabStripSceneLayer.this,
                        layoutHelper.getRightFadeDrawable(),
                        layoutHelper.getRightFadeOpacity(),
                        resourceManager,
                        layoutHelper.getBackgroundColor(),
                        rightPaddingPx);
    }

    @VisibleForTesting
    /* package */ void pushStripTabs(
            StripLayoutHelperManager layoutHelper,
            LayerTitleCache layerTitleCache,
            ResourceManager resourceManager,
            StripLayoutTab[] stripTabs,
            int selectedTabId,
            int hoveredTabId) {
        final int tabsCount = stripTabs != null ? stripTabs.length : 0;

        // TODO(crbug.com/40270147): Cleanup params, as some don't change and others are now
        //  unused.
        for (int i = 0; i < tabsCount; i++) {
            final StripLayoutTab st = stripTabs[i];
            boolean isSelected = st.getTabId() == selectedTabId;
            boolean isHovered = st.getTabId() == hoveredTabId;
            boolean shouldShowOutline = layoutHelper.shouldShowTabOutline(st);
            @DrawableRes
            int focusBackground =
                    isSelected && shouldShowOutline
                            ? TabUiThemeUtil.getSelectedTabInTabGroupKeyboardFocusDrawableRes()
                            : TabUiThemeUtil.getTabKeyboardFocusDrawableRes();

            // TODO(crbug.com/326301060): Update tab outline placeholder color with color picker.
            TabStripSceneLayerJni.get()
                    .putStripTabLayer(
                            mNativePtr,
                            TabStripSceneLayer.this,
                            st.getTabId(),
                            st.getCloseButton().getResourceId(),
                            st.getCloseButton().getBackgroundResourceId(),
                            st.getCloseButton().isKeyboardFocused(),
                            TabUiThemeUtil.getCircularButtonKeyboardFocusDrawableRes(),
                            st.getDividerResourceId(),
                            st.getResourceId(),
                            st.getOutlineResourceId(),
                            st.getCloseButton().getTint(),
                            st.getCloseButton().getBackgroundTint(),
                            st.getDividerTint(),
                            st.getTint(isSelected, isHovered),
                            layoutHelper.getSelectedOutlineGroupTint(
                                    st.getTabId(), shouldShowOutline),
                            st.isForegrounded(),
                            shouldShowOutline,
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
                            st.getContainerOpacity(),
                            st.isKeyboardFocused(),
                            focusBackground,
                            st.getKeyboardFocusRingColor(),
                            st.getKeyboardFocusRingOffset(),
                            st.getLineWidth(),
                            FOLIO_FOOT_LENGTH_DP * mDpToPx,
                            layerTitleCache,
                            resourceManager);
        }
    }

    /* package */ void pushGroupIndicators(
            StripLayoutGroupTitle[] groupTitles,
            LayerTitleCache layerTitleCache,
            ResourceManager resourceManager) {
        final int titlesCount = groupTitles != null ? groupTitles.length : 0;

        for (int i = 0; i < titlesCount; i++) {
            final StripLayoutGroupTitle gt = groupTitles[i];

            TabStripSceneLayerJni.get()
                    .putGroupIndicatorLayer(
                            mNativePtr,
                            TabStripSceneLayer.this,
                            gt.isIncognito(),
                            gt.isForegrounded(),
                            gt.isCollapsed(),
                            gt.getNotificationBubbleShown(),
                            gt.getTabGroupId(),
                            gt.getTint(),
                            gt.getReorderBackgroundTint(),
                            gt.getBubbleTint(),
                            gt.getPaddedX() * mDpToPx,
                            gt.getPaddedY() * mDpToPx,
                            gt.getPaddedWidth() * mDpToPx,
                            gt.getPaddedHeight() * mDpToPx,
                            gt.getTitleStartPadding() * mDpToPx,
                            gt.getTitleEndPadding() * mDpToPx,
                            gt.getCornerRadius() * mDpToPx,
                            gt.getBottomIndicatorWidth() * mDpToPx,
                            gt.getBottomIndicatorHeight() * mDpToPx,
                            gt.getBubblePadding() * mDpToPx,
                            gt.getBubbleSize() * mDpToPx,
                            gt.isKeyboardFocused(),
                            TabUiThemeUtil.getTabGroupIndicatorKeyboardFocusDrawableRes(),
                            gt.getKeyboardFocusRingColor(),
                            gt.getKeyboardFocusRingOffset(),
                            gt.getKeyboardFocusRingWidth(),
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

        void setConstants(
                long nativeTabStripSceneLayer,
                int reorderBackgroundTopMargin,
                int reorderBackgroundBottomMargin,
                int reorderBackgroundPaddingShort,
                int reorderBackgroundPaddingLong,
                int reorderBackgroundCornerRadius);

        void beginBuildingFrame(
                long nativeTabStripSceneLayer, TabStripSceneLayer caller, boolean visible);

        void finishBuildingFrame(long nativeTabStripSceneLayer, TabStripSceneLayer caller);

        void updateOffsetTag(
                long nativeTabStripSceneLayer, TabStripSceneLayer caller, OffsetTag offsetTag);

        void updateTabStripLayer(
                long nativeTabStripSceneLayer,
                TabStripSceneLayer caller,
                int width,
                int height,
                float yOffset,
                @ColorInt int backgroundColor,
                @ColorInt int scrimColor,
                float scrimOpacity,
                float leftPaddingPx,
                float rightPaddingPx,
                float topPaddingPx);

        void updateNewTabButton(
                long nativeTabStripSceneLayer,
                TabStripSceneLayer caller,
                int resourceId,
                int backgroundResourceId,
                float x,
                float y,
                float touchTargetOffset,
                boolean visible,
                boolean isHovered,
                int tint,
                int backgroundTint,
                float buttonAlpha,
                boolean isKeyboardFocused,
                int keyboardFocusRingResourceId,
                int keyboardFocusRingColor,
                ResourceManager resourceManager);

        void updateModelSelectorButton(
                long nativeTabStripSceneLayer,
                TabStripSceneLayer caller,
                int resourceId,
                int backgroundResourceId,
                float x,
                float y,
                boolean visible,
                boolean isHovered,
                int tint,
                int backgroundTint,
                float buttonAlpha,
                boolean isKeyboardFocused,
                int keyboardFocusRingResourceId,
                int keyboardFocusRingColor,
                ResourceManager resourceManager);

        void updateTabStripLeftFade(
                long nativeTabStripSceneLayer,
                TabStripSceneLayer caller,
                int resourceId,
                float opacity,
                ResourceManager resourceManager,
                @ColorInt int leftFadeColor,
                float leftPaddingPx);

        void updateTabStripRightFade(
                long nativeTabStripSceneLayer,
                TabStripSceneLayer caller,
                int resourceId,
                float opacity,
                ResourceManager resourceManager,
                @ColorInt int rightFadeColor,
                float rightPaddingPx);

        void putStripTabLayer(
                long nativeTabStripSceneLayer,
                TabStripSceneLayer caller,
                int id,
                int closeResourceId,
                int closeBackgroundResourceId,
                boolean isCloseKeyboardFocused,
                int closeFocusRingResourceId,
                int dividerResourceId,
                int handleResourceId,
                int handleOutlineResourceId,
                int closeTint,
                int closeHoverBackgroundTint,
                int dividerTint,
                int handleTint,
                int handleOutlineTint,
                boolean foreground,
                boolean shouldShowTabOutline,
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
                float opacity,
                boolean isKeyboardFocused,
                int keyboardFocusRingResourceId,
                int keyboardFocusRingColor,
                int keyboardFocusRingOffset,
                int strokeWidth,
                float folioFootLength,
                LayerTitleCache layerTitleCache,
                ResourceManager resourceManager);

        void putGroupIndicatorLayer(
                long nativeTabStripSceneLayer,
                TabStripSceneLayer caller,
                boolean incognito,
                boolean foreground,
                boolean collapsed,
                boolean showBubble,
                Token groupToken,
                int tint,
                int reorderBackgroundTint,
                int bubbleTint,
                float x,
                float y,
                float width,
                float height,
                float titleStartPadding,
                float titleEndPadding,
                float cornerRadius,
                float bottomIndicatorWidth,
                float bottomIndicatorHeight,
                float bubblePadding,
                float bubbleSize,
                boolean isKeyboardFocused,
                int keyboardFocusRingResourceId,
                int keyboardFocusRingColor,
                int keyboardFocusRingOffset,
                int keyboardFocusRingWidth,
                LayerTitleCache layerTitleCache,
                ResourceManager resourceManager);

        void setContentTree(
                long nativeTabStripSceneLayer, TabStripSceneLayer caller, SceneLayer contentTree);
    }

    public void initializeNativeForTesting() {
        this.initializeNative();
    }
}
