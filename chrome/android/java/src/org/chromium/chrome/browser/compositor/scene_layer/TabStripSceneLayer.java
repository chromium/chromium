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
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.cc.input.OffsetTag;
import org.chromium.chrome.browser.compositor.LayerTitleCache;
import org.chromium.chrome.browser.compositor.layouts.components.CompositorButton;
import org.chromium.chrome.browser.compositor.layouts.components.TintedCompositorButton;
import org.chromium.chrome.browser.compositor.layouts.components.TintedCompositorTextButton;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutGroupTitle;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutHelperManager;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutTab;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutTrailingButtonsCoordinator;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutUtils;
import org.chromium.chrome.browser.layouts.scene_layer.SceneLayer;
import org.chromium.chrome.browser.layouts.scene_layer.SceneOverlayLayer;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tasks.tab_management.TabUiThemeUtil;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.ui.resources.ResourceManager;
import org.chromium.ui.util.StyleUtils;

/**
 * The Java component of what is basically a CC Layer that manages drawing the Tab Strip (which is
 * composed of {@link StripLayoutTab}s) to the screen. This object keeps the layers up to date and
 * removes/creates children as necessary. This object is built by its native counterpart.
 */
@JNINamespace("android")
@NullMarked
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
                                StripLayoutGroupTitle.REORDER_BACKGROUND_CORNER_RADIUS * mDpToPx),
                        StripLayoutTab.TAB_UNDERLINE_THICKNESS_DP * mDpToPx,
                        StripLayoutTab.TAB_UNDERLINE_CORNER_RADIUS_DP * mDpToPx,
                        StripLayoutTab.TAB_UNDERLINE_BOTTOM_MARGIN_DP * mDpToPx);
    }

    public static void setTestFlag(boolean testFlag) {
        sTestFlag = testFlag;
    }

    @Override
    protected void initializeNative() {
        if (mNativePtr == 0) {
            mNativePtr = TabStripSceneLayerJni.get().init(this);
        }
        // Set flag for testing
        if (!sTestFlag) {
            assert mNativePtr != 0;
        }
    }

    @Override
    public void setContentTree(SceneLayer contentTree) {
        TabStripSceneLayerJni.get().setContentTree(mNativePtr, contentTree);
    }

    /**
     * Pushes all relevant {@link StripLayoutTab}s to the CC Layer tree. This also pushes any other
     * assets required to draw the Tab Strip. This should only be called when the Compositor has
     * disabled ScheduleComposite calls as this will change the tree and could subsequently cause
     * unnecessary follow up renders.
     *
     * @param layoutHelper A layout helper for the tab strip.
     * @param trailingButtonsCoordinator The coordinator for trailing buttons like Glic.
     * @param layerTitleCache A layer title cache.
     * @param resourceManager A resource manager.
     * @param stripLayoutTabsToRender Array of strip layout tabs.
     * @param stripLayoutGroupTitlesToRender Array of strip layout group titles.
     * @param yOffset Current browser controls offset in dp.
     * @param selectedTabId The ID of the selected tab.
     * @param hoveredTabId The ID of the hovered tab, if any. If no tab is hovered on, this ID will
     *     be invalid.
     * @param scrimColor The color of the scrim overlay that covers the tab strip.
     * @param scrimOpacity The opacity of the scrim overlay that covers the tab strip.
     * @param leftPaddingDp The left padding for the tab strip in dp.
     * @param rightPaddingDp The right padding for the tab strip in dp.
     * @param topPaddingDp The top padding for the tab strip in dp.
     */
    public void pushAndUpdateStrip(
            StripLayoutHelperManager layoutHelper,
            StripLayoutTrailingButtonsCoordinator trailingButtonsCoordinator,
            LayerTitleCache layerTitleCache,
            ResourceManager resourceManager,
            StripLayoutTab[] stripLayoutTabsToRender,
            StripLayoutGroupTitle[] stripLayoutGroupTitlesToRender,
            float yOffset,
            @TabId int selectedTabId,
            @TabId int hoveredTabId,
            @ColorInt int scrimColor,
            float scrimOpacity,
            float leftPaddingDp,
            float rightPaddingDp,
            float topPaddingDp) {

        if (mNativePtr == 0) return;
        final boolean visible = yOffset > -layoutHelper.getHeight();

        // This will hide the tab strips if necessary.
        TabStripSceneLayerJni.get()
                .beginBuildingFrame(mNativePtr, visible, resourceManager, layerTitleCache);
        // When strip tabs are completely off screen, we don't need to update it.
        if (visible) {
            // Ceil the padding to avoid off-by-one issues similar to crbug/329722454. This is
            // required since these values are originated from Android UI.
            float leftPaddingPx = (float) Math.ceil(leftPaddingDp * mDpToPx);
            float rightPaddingPx = (float) Math.ceil(rightPaddingDp * mDpToPx);
            float topPaddingPx = (float) Math.ceil(topPaddingDp * mDpToPx);

            pushButtonsAndBackground(
                    layoutHelper,
                    trailingButtonsCoordinator,
                    yOffset,
                    scrimColor,
                    scrimOpacity,
                    leftPaddingPx,
                    rightPaddingPx,
                    topPaddingPx);
            pushGroupIndicators(stripLayoutGroupTitlesToRender, layerTitleCache);
            pushStripTabs(layoutHelper, layerTitleCache, stripLayoutTabsToRender, selectedTabId);
        }
        TabStripSceneLayerJni.get().finishBuildingFrame(mNativePtr);
    }

    public void updateOffsetTag(@Nullable OffsetTag offsetTag) {
        TabStripSceneLayerJni.get().updateOffsetTag(mNativePtr, offsetTag);
    }

    @VisibleForTesting
    /* package */ void pushButtonsAndBackground(
            StripLayoutHelperManager layoutHelper,
            StripLayoutTrailingButtonsCoordinator trailingButtonsCoordinator,
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
                        width,
                        height,
                        Math.round(yOffset * mDpToPx),
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
                        newTabButton.getResourceId(),
                        newTabButton.getBackgroundResourceId(),
                        Math.round(newTabButton.getDrawX() * mDpToPx),
                        Math.round(newTabButton.getDrawY() * mDpToPx),
                        Math.round(layoutHelper.getNewTabBtnVisualOffset() * mDpToPx),
                        newTabButtonVisible,
                        newTabButton.getShouldApplyHoverBackground(),
                        newTabButton.getTint(),
                        newTabButton.getBackgroundTint(),
                        newTabButton.getOpacity(),
                        newTabButton.isKeyboardFocused(),
                        TabUiThemeUtil.getCircularButtonKeyboardFocusDrawableRes(),
                        newTabButton.getKeyboardFocusRingColor());

        TintedCompositorTextButton glicButton = trailingButtonsCoordinator.getGlicButton();
        TintedCompositorTextButton glicActorButton =
                trailingButtonsCoordinator.getGlicActorButton();
        boolean glicActorVisible = glicActorButton != null && glicActorButton.isVisible();
        if (glicButton != null && glicButton.getDismissButton() != null) {
            TintedCompositorButton dismissNudge = glicButton.getDismissButton();
            boolean glicButtonVisible = glicButton.isVisible();
            boolean dismissVisible = dismissNudge.isVisible() && glicButtonVisible;
            int glicButtonStartPadding =
                    Math.round(
                            StripLayoutTrailingButtonsCoordinator.GLIC_BUTTON_START_PADDING_DP
                                    * mDpToPx);
            int glicIconTextPadding =
                    Math.round(
                            StripLayoutTrailingButtonsCoordinator.GLIC_ICON_TEXT_PADDING_DP
                                    * mDpToPx);
            float glicCornerRadiusOuter =
                    StripLayoutTrailingButtonsCoordinator.GLIC_BUTTON_CORNER_RADIUS_DP;
            float glicCornerRadiusInner = glicCornerRadiusOuter;
            if (glicActorVisible) {
                glicCornerRadiusInner =
                        StripLayoutTrailingButtonsCoordinator.GLIC_BUTTON_INNER_CORNER_RADIUS_DP;
            }

            TabStripSceneLayerJni.get()
                    .updateGlicButton(
                            mNativePtr,
                            glicButton.getResourceId(),
                            Math.round(glicButton.getDrawX() * mDpToPx),
                            Math.round(glicButton.getDrawY() * mDpToPx),
                            Math.round(glicButton.getWidth() * mDpToPx),
                            Math.round(glicButton.getHeight() * mDpToPx),
                            glicButtonVisible,
                            glicButton.getShouldApplyHoverBackground(),
                            glicButton.getTint(),
                            /* shouldTint= */ false,
                            glicButton.getBackgroundTint(),
                            glicButton.getOpacity(),
                            glicButton.isKeyboardFocused(),
                            TabUiThemeUtil.getGlicButtonKeyboardFocusDrawableRes(glicActorVisible),
                            glicButton.getKeyboardFocusRingColor(),
                            TabUiThemeUtil.getFocusRingOffset(layoutHelper.getContext()),
                            glicButton.getTextResourceId(),
                            glicButtonStartPadding,
                            glicIconTextPadding,
                            Math.round(glicCornerRadiusOuter * mDpToPx),
                            Math.round(glicCornerRadiusInner * mDpToPx),
                            dismissNudge.getResourceId(),
                            Math.round(dismissNudge.getDrawX() * mDpToPx),
                            Math.round(dismissNudge.getDrawY() * mDpToPx),
                            dismissVisible,
                            dismissNudge.getTint(),
                            dismissNudge.isKeyboardFocused(),
                            TabUiThemeUtil.getCircularButtonKeyboardFocusDrawableRes(),
                            dismissNudge.getKeyboardFocusRingColor());
        }

        if (glicActorButton != null) {
            boolean glicActorButtonVisible = glicActorVisible;
            int glicActorButtonStartPadding =
                    Math.round(
                            StripLayoutTrailingButtonsCoordinator.GLIC_BUTTON_START_PADDING_DP
                                    * mDpToPx);
            int glicActorIconTextPadding =
                    Math.round(
                            StripLayoutTrailingButtonsCoordinator.GLIC_ICON_TEXT_PADDING_DP
                                    * mDpToPx);
            float actorCornerRadiusOuter =
                    StripLayoutTrailingButtonsCoordinator.GLIC_BUTTON_INNER_CORNER_RADIUS_DP
                            * mDpToPx;
            float actorCornerRadiusInner =
                    StripLayoutTrailingButtonsCoordinator.GLIC_BUTTON_CORNER_RADIUS_DP * mDpToPx;

            TabStripSceneLayerJni.get()
                    .updateGlicActorButton(
                            mNativePtr,
                            glicActorButton.getResourceId(),
                            Math.round(glicActorButton.getDrawX() * mDpToPx),
                            Math.round(glicActorButton.getDrawY() * mDpToPx),
                            Math.round(glicActorButton.getWidth() * mDpToPx),
                            Math.round(glicActorButton.getHeight() * mDpToPx),
                            glicActorButtonVisible,
                            glicActorButton.getShouldApplyHoverBackground(),
                            glicActorButton.getTint(),
                            /* shouldTint= */ true,
                            glicActorButton.getBackgroundTint(),
                            glicActorButton.getOpacity(),
                            glicActorButton.isKeyboardFocused(),
                            TabUiThemeUtil.getGlicButtonKeyboardFocusDrawableRes(
                                    /* isSplit= */ true),
                            glicActorButton.getKeyboardFocusRingColor(),
                            TabUiThemeUtil.getFocusRingOffset(layoutHelper.getContext()),
                            glicActorButton.getTextResourceId(),
                            glicActorButtonStartPadding,
                            glicActorIconTextPadding,
                            actorCornerRadiusOuter,
                            actorCornerRadiusInner);
        }

        CompositorButton modelSelectorButton = layoutHelper.getModelSelectorButton();
        if (modelSelectorButton != null) {
            boolean modelSelectorButtonVisible = modelSelectorButton.isVisible();
            TabStripSceneLayerJni.get()
                    .updateModelSelectorButton(
                            mNativePtr,
                            modelSelectorButton.getResourceId(),
                            ((TintedCompositorButton) modelSelectorButton)
                                    .getBackgroundResourceId(),
                            Math.round(modelSelectorButton.getDrawX() * mDpToPx),
                            Math.round(modelSelectorButton.getDrawY() * mDpToPx),
                            modelSelectorButtonVisible,
                            modelSelectorButton.getShouldApplyHoverBackground(),
                            ((TintedCompositorButton) modelSelectorButton).getTint(),
                            ((TintedCompositorButton) modelSelectorButton).getBackgroundTint(),
                            modelSelectorButton.getOpacity(),
                            modelSelectorButton.isKeyboardFocused(),
                            TabUiThemeUtil.getCircularButtonKeyboardFocusDrawableRes(),
                            modelSelectorButton.getKeyboardFocusRingColor());
        }

        TabStripSceneLayerJni.get()
                .updateTabStripFade(
                        mNativePtr,
                        /* isLeft= */ true,
                        layoutHelper.getBackgroundColor(),
                        layoutHelper.getLeftFadeOpacity(),
                        Math.round(layoutHelper.getLeftFadeGradientWidth() * mDpToPx),
                        Math.round(layoutHelper.getLeftFadeOpaqueWidth() * mDpToPx),
                        leftPaddingPx);

        TabStripSceneLayerJni.get()
                .updateTabStripFade(
                        mNativePtr,
                        /* isLeft= */ false,
                        layoutHelper.getBackgroundColor(),
                        layoutHelper.getRightFadeOpacity(),
                        Math.round(layoutHelper.getRightFadeGradientWidth() * mDpToPx),
                        Math.round(layoutHelper.getRightFadeOpaqueWidth() * mDpToPx),
                        rightPaddingPx);
    }

    @VisibleForTesting
    /* package */ void pushStripTabs(
            StripLayoutHelperManager layoutHelper,
            LayerTitleCache layerTitleCache,
            StripLayoutTab[] stripTabs,
            @TabId int selectedTabId) {
        final int tabsCount = stripTabs != null ? stripTabs.length : 0;
        @ColorInt
        int underlineStartColor =
                TabUiThemeUtil.getTabUnderlineGradientStart(layoutHelper.getContext());
        int underlineEndColor =
                TabUiThemeUtil.getTabUnderlineGradientEnd(layoutHelper.getContext());

        // TODO(crbug.com/40270147): Cleanup params, as some don't change and others are now
        //  unused.
        for (int i = 0; i < tabsCount; i++) {
            final StripLayoutTab st = stripTabs[i];
            boolean isSelected = st.getTabId() == selectedTabId;
            boolean shouldShowOutline = layoutHelper.shouldShowTabOutline(st);
            @DrawableRes
            int focusBackground =
                    isSelected && shouldShowOutline
                            ? TabUiThemeUtil.getSelectedTabInTabGroupKeyboardFocusDrawableRes()
                            : TabUiThemeUtil.getTabKeyboardFocusDrawableRes();
            TintedCompositorButton closeButton = st.getCloseButton();
            @ColorInt int closeButtonTint = closeButton.getTint();

            boolean shouldShowIndicator = st.shouldShowIndicator();

            boolean isPinned = st.getIsPinned();
            float widthToHideTabTitle =
                    (StyleUtils.shouldApplyDesktopDensity() || isPinned)
                            ? StripLayoutUtils.MIN_TAB_WIDTH_DP
                            : 0.f;

            TabStripSceneLayerJni.get()
                    .putStripTabLayer(
                            mNativePtr,
                            st.getTabId(),
                            closeButton.getResourceId(),
                            closeButton.getBackgroundResourceId(),
                            closeButton.isKeyboardFocused(),
                            TabUiThemeUtil.getCircularButtonKeyboardFocusDrawableRes(),
                            st.getDividerResourceId(),
                            st.getResourceId(),
                            st.getOutlineResourceId(),
                            closeButtonTint,
                            closeButton.getBackgroundTint(),
                            st.getDividerTint(),
                            st.getTint(),
                            layoutHelper.getSelectedOutlineGroupTint(
                                    st.getTabId(), shouldShowOutline),
                            st.isForegrounded(),
                            shouldShowOutline,
                            st.getClosePressed(),
                            st.shouldHideFavicon(shouldShowIndicator),
                            shouldShowIndicator,
                            st.getIndicatorRes(),
                            st.getIndicatorTint(),
                            Math.round(st.getMediaIndicatorWidth() * mDpToPx),
                            Math.round(st.getMediaIndicatorToCloseButtonSpacing() * mDpToPx),
                            Math.round(st.getMediaIndicatorInternalPadding() * mDpToPx),
                            Math.round(st.getTitleToMediaIndicatorSpacing() * mDpToPx),
                            st.getIndicatorOverlayRes(),
                            st.getTabIndicatorOverlayRotation(),
                            Math.round(st.getTabIndicatorOverlayWidth() * mDpToPx),
                            Math.round(layoutHelper.getWidth() * mDpToPx),
                            Math.round(st.getDrawX() * mDpToPx),
                            Math.round(st.getDrawY() * mDpToPx),
                            Math.round(st.getWidth() * mDpToPx),
                            Math.round(st.getHeight() * mDpToPx),
                            Math.round(st.getContentOffsetY() * mDpToPx),
                            Math.round(st.getDividerOffsetX() * mDpToPx),
                            Math.round(st.getBottomMargin() * mDpToPx),
                            Math.round(st.getTopMargin() * mDpToPx),
                            Math.round(st.getCloseButtonPadding() * mDpToPx),
                            closeButton.getOpacity(),
                            Math.round(widthToHideTabTitle * mDpToPx),
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
                            Math.round(FOLIO_FOOT_LENGTH_DP * mDpToPx),
                            isPinned,
                            Math.round(st.getPinnedTabFaviconOffsetX() * mDpToPx),
                            st.isUnderlined(),
                            underlineStartColor,
                            underlineEndColor,
                            Math.round(StripLayoutTab.FAVICON_WIDTH * 2 * mDpToPx));
        }
    }

    /* package */ void pushGroupIndicators(
            StripLayoutGroupTitle[] groupTitles, LayerTitleCache layerTitleCache) {
        final int titlesCount = groupTitles != null ? groupTitles.length : 0;

        for (int i = 0; i < titlesCount; i++) {
            final StripLayoutGroupTitle gt = groupTitles[i];

            TabStripSceneLayerJni.get()
                    .putGroupIndicatorLayer(
                            mNativePtr,
                            gt.isIncognito(),
                            gt.isForegrounded(),
                            gt.isCollapsed(),
                            gt.getNotificationBubbleShown(),
                            gt.getTabGroupId(),
                            gt.getTint(),
                            gt.getReorderBackgroundTint(),
                            gt.getBubbleTint(),
                            Math.round(gt.getPaddedX() * mDpToPx),
                            Math.round(gt.getPaddedY() * mDpToPx),
                            Math.round(gt.getPaddedWidth() * mDpToPx),
                            Math.round(gt.getPaddedHeight() * mDpToPx),
                            Math.round(gt.getTitleStartPadding() * mDpToPx),
                            Math.round(gt.getTitleEndPadding() * mDpToPx),
                            Math.round(gt.getCornerRadius() * mDpToPx),
                            Math.round(gt.getBottomIndicatorWidth() * mDpToPx),
                            Math.round(gt.getBottomIndicatorHeight() * mDpToPx),
                            Math.round(gt.getBubblePadding() * mDpToPx),
                            Math.round(gt.getBubbleSize() * mDpToPx),
                            gt.isKeyboardFocused(),
                            TabUiThemeUtil.getTabGroupIndicatorKeyboardFocusDrawableRes(),
                            gt.getKeyboardFocusRingColor(),
                            gt.getKeyboardFocusRingOffset(),
                            gt.getKeyboardFocusRingWidth());
        }
    }

    @Override
    public void destroy() {
        super.destroy();
        mNativePtr = 0;
    }

    @NativeMethods
    public interface Natives {
        long init(TabStripSceneLayer self);

        void setConstants(
                long nativeTabStripSceneLayer,
                int reorderBackgroundTopMargin,
                int reorderBackgroundBottomMargin,
                int reorderBackgroundPaddingShort,
                int reorderBackgroundPaddingLong,
                int reorderBackgroundCornerRadius,
                float tabUnderlineThickness,
                float tabUnderlineCornerRadius,
                float tabUnderlineBottomMargin);

        void beginBuildingFrame(
                long nativeTabStripSceneLayer,
                boolean visible,
                ResourceManager resourceManager,
                LayerTitleCache layerTitleCache);

        void finishBuildingFrame(long nativeTabStripSceneLayer);

        void updateOffsetTag(long nativeTabStripSceneLayer, @Nullable OffsetTag offsetTag);

        void updateTabStripLayer(
                long nativeTabStripSceneLayer,
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
                @DrawableRes int resourceId,
                @DrawableRes int backgroundResourceId,
                float x,
                float y,
                float touchTargetOffset,
                boolean visible,
                boolean isHovered,
                @ColorInt int tint,
                @ColorInt int backgroundTint,
                float buttonAlpha,
                boolean isKeyboardFocused,
                @DrawableRes int keyboardFocusRingResourceId,
                @ColorInt int keyboardFocusRingColor);

        void updateGlicButton(
                long nativeTabStripSceneLayer,
                @DrawableRes int resourceId,
                float x,
                float y,
                float buttonWidth,
                float buttonHeight,
                boolean visible,
                boolean isHovered,
                @ColorInt int tint,
                boolean shouldTint,
                @ColorInt int backgroundTint,
                float buttonAlpha,
                boolean isKeyboardFocused,
                @DrawableRes int keyboardFocusRingResourceId,
                @ColorInt int keyboardFocusRingColor,
                float keyboardFocusRingOffset,
                int textTextureId,
                float buttonStartPadding,
                float buttonTextPadding,
                float cornerRadiusOuter,
                float cornerRadiusInner,
                @DrawableRes int dismissResourceId,
                float dismissX,
                float dismissY,
                boolean dismissVisible,
                @ColorInt int dismissTint,
                boolean dismissIsKeyboardFocused,
                @DrawableRes int dismissKeyboardFocusRingResourceId,
                @ColorInt int dismissKeyboardFocusRingColor);

        void updateGlicActorButton(
                long nativeTabStripSceneLayer,
                @DrawableRes int resourceId,
                float x,
                float y,
                float buttonWidth,
                float buttonHeight,
                boolean visible,
                boolean isHovered,
                @ColorInt int tint,
                boolean shouldTint,
                @ColorInt int backgroundTint,
                float buttonAlpha,
                boolean isKeyboardFocused,
                @DrawableRes int keyboardFocusRingResourceId,
                @ColorInt int keyboardFocusRingColor,
                float keyboardFocusRingOffset,
                int textTextureId,
                float buttonStartPadding,
                float buttonTextPadding,
                float cornerRadiusOuter,
                float cornerRadiusInner);

        void updateModelSelectorButton(
                long nativeTabStripSceneLayer,
                @DrawableRes int resourceId,
                @DrawableRes int backgroundResourceId,
                float x,
                float y,
                boolean visible,
                boolean isHovered,
                @ColorInt int tint,
                @ColorInt int backgroundTint,
                float buttonAlpha,
                boolean isKeyboardFocused,
                @DrawableRes int keyboardFocusRingResourceId,
                @ColorInt int keyboardFocusRingColor);

        void updateTabStripFade(
                long nativeTabStripSceneLayer,
                boolean isLeft,
                @ColorInt int fadeColor,
                float opacity,
                float gradientWidthPx,
                float opaqueWidthPx,
                float paddingPx);

        void putStripTabLayer(
                long nativeTabStripSceneLayer,
                @TabId int id,
                @DrawableRes int closeResourceId,
                @DrawableRes int closeBackgroundResourceId,
                boolean isCloseKeyboardFocused,
                @DrawableRes int closeFocusRingResourceId,
                @DrawableRes int dividerResourceId,
                @DrawableRes int handleResourceId,
                @DrawableRes int handleOutlineResourceId,
                @ColorInt int closeTint,
                @ColorInt int closeHoverBackgroundTint,
                @ColorInt int dividerTint,
                @ColorInt int handleTint,
                @ColorInt int handleOutlineTint,
                boolean foreground,
                boolean shouldShowTabOutline,
                boolean closePressed,
                boolean shouldHideFavicon,
                boolean shouldShowMediaIndicator,
                @DrawableRes int mediaIndicatorResourceId,
                @ColorInt int mediaIndicatorTint,
                float mediaIndicatorWidth,
                float mediaIndicatorSpacing,
                float mediaIndicatorInternalPadding,
                float titleToMediaIndicatorSpacing,
                @DrawableRes int tabIndicatorOverlayResourceId,
                float tabIndicatorOverlayRotation,
                float tabIndicatorOverlayWidth,
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
                float widthToHideTabTitle,
                boolean isStartDividerVisible,
                boolean isEndDividerVisible,
                boolean isLoading,
                float spinnerRotation,
                float opacity,
                boolean isKeyboardFocused,
                @DrawableRes int keyboardFocusRingResourceId,
                @ColorInt int keyboardFocusRingColor,
                int keyboardFocusRingOffset,
                int strokeWidth,
                float folioFootLength,
                boolean isPinned,
                float pinnedIconOffsetX,
                boolean isUnderlined,
                @ColorInt int underlineStartColor,
                @ColorInt int underlineEndColor,
                int underlineWidthThreshold);

        void putGroupIndicatorLayer(
                long nativeTabStripSceneLayer,
                boolean incognito,
                boolean foreground,
                boolean collapsed,
                boolean showBubble,
                Token groupToken,
                @TabGroupColorId int tint,
                @ColorInt int reorderBackgroundTint,
                @ColorInt int bubbleTint,
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
                @DrawableRes int keyboardFocusRingResourceId,
                @ColorInt int keyboardFocusRingColor,
                int keyboardFocusRingOffset,
                int keyboardFocusRingWidth);

        void setContentTree(long nativeTabStripSceneLayer, SceneLayer contentTree);
    }

    public void initializeNativeForTesting() {
        this.initializeNative();
    }
}
