// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.scene_layer;

import androidx.annotation.ColorInt;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.bottombar.contextualsearch.ContextualSearchBarControl;
import org.chromium.chrome.browser.compositor.bottombar.contextualsearch.ContextualSearchImageControl;
import org.chromium.chrome.browser.compositor.bottombar.contextualsearch.ContextualSearchPanel;
import org.chromium.chrome.browser.compositor.bottombar.contextualsearch.ContextualSearchPromoControl;
import org.chromium.chrome.browser.compositor.bottombar.contextualsearch.RelatedSearchesControl;
import org.chromium.chrome.browser.layouts.scene_layer.SceneLayer;
import org.chromium.chrome.browser.layouts.scene_layer.SceneOverlayLayer;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.resources.ResourceManager;

/** A SceneLayer to render layers for ContextualSearchLayout. */
@JNINamespace("android")
public class ContextualSearchSceneLayer extends SceneOverlayLayer {
    // NOTE: If you use SceneLayer's native pointer here, the JNI generator will try to
    // downcast using reinterpret_cast<>. We keep a separate pointer to avoid it.
    private long mNativePtr;

    /** If the scene layer has been initialized. */
    private boolean mIsInitialized;

    private final Profile mProfile;
    private final float mDpToPx;

    private ContextualSearchImageControl mImageControl;

    public ContextualSearchSceneLayer(Profile profile, float dpToPx) {
        mProfile = profile;
        mDpToPx = dpToPx;
    }

    /**
     * Update the scene layer to draw an OverlayPanel.
     * @param resourceManager Manager to get view and image resources.
     * @param panel The OverlayPanel to render.
     * @param searchBarControl The Search Bar control.
     * @param promoControl The privacy Opt-in promo that appears below the Bar.
     * @param relatedSearchesInBarControl A control that displays Related Searches suggestions
     *        in the Bar to facilitate one-click searching.
     * @param imageControl The object controlling the image displayed in the Bar.
     */
    public void update(
            ResourceManager resourceManager,
            ContextualSearchPanel panel,
            ContextualSearchBarControl searchBarControl,
            ContextualSearchPromoControl promoControl,
            RelatedSearchesControl relatedSearchesInBarControl,
            ContextualSearchImageControl imageControl) {
        // Don't try to update the layer if not initialized or showing.
        if (resourceManager == null || !panel.isShowing()) return;

        if (!mIsInitialized) {
            ContextualSearchSceneLayerJni.get()
                    .createContextualSearchLayer(mNativePtr, resourceManager);
            mIsInitialized = true;
        }
        mImageControl = imageControl;

        final int searchBarBackgroundColor = panel.getBarBackgroundColor();
        int searchContextViewId = searchBarControl.getSearchContextViewId();
        int searchTermViewId = searchBarControl.getSearchTermViewId();
        int searchCaptionViewId = searchBarControl.getCaptionViewId();

        int openNewTabIconId =
                panel.canPromoteToNewTab() ? R.drawable.open_in_new_tab : INVALID_RESOURCE_ID;
        int dragHandlebarId = R.drawable.drag_handlebar;

        int searchPromoViewId = promoControl.getViewId();
        boolean searchPromoVisible = promoControl.isVisible();
        float searchPromoHeightPx = promoControl.getHeightPx();
        float searchPromoOpacity = promoControl.getOpacity();
        int searchPromoBackgroundColor = promoControl.getBackgroundColor();

        // Related Searches section
        int relatedSearchesInBarViewId = relatedSearchesInBarControl.getViewId();
        boolean relatedSearchesInBarVisible = relatedSearchesInBarControl.isVisible();
        // We already have a margin below the text in the Bar, but the RelatedSearches section has
        // its own top and bottom margin, so the below-text margin is redundant.
        float relatedSearchesInBarRedundantPadding =
                panel.getInBarRelatedSearchesRedundantPadding();
        float relatedSearchesInBarHeight =
                panel.getInBarRelatedSearchesAnimatedHeightDps() * mDpToPx
                        - relatedSearchesInBarRedundantPadding;

        float customImageVisibilityPercentage = imageControl.getCustomImageVisibilityPercentage();
        int barImageSize = imageControl.getBarImageSize();

        boolean quickActionIconVisible = imageControl.getCardIconVisible();
        int quickActionIconResId = imageControl.getCardIconResourceId();

        boolean thumbnailVisible = imageControl.getThumbnailVisible();
        String thumbnailUrl = imageControl.getThumbnailUrl();

        float searchPanelX = panel.getOffsetX();
        float searchPanelY = panel.getOffsetY();
        float searchPanelWidth = panel.getWidth();
        float searchPanelHeight = panel.getHeight();

        float searchCaptionAnimationPercentage = searchBarControl.getCaptionAnimationPercentage();
        boolean searchCaptionVisible = searchBarControl.getCaptionVisible();

        float searchBarMarginSide = panel.getBarMarginSide();
        float searchBarMarginTop = panel.getBarMarginTop();
        float baseSearchBarMarginBottom = panel.getBarMarginBottomPx();
        float searchBarMarginBottom = baseSearchBarMarginBottom * searchCaptionAnimationPercentage;
        float searchBarHeight = panel.getBarHeight();
        // By default, the search bar height includes the entire bottom margin. Remove any excess
        // based on the animation percentage.
        searchBarHeight -=
                (baseSearchBarMarginBottom * (1 - searchCaptionAnimationPercentage) / mDpToPx);

        float searchContextOpacity = searchBarControl.getSearchBarContextOpacity();
        float searchTermOpacity = searchBarControl.getSearchBarTermOpacity();

        boolean searchBarBorderVisible = panel.isBarBorderVisible();
        float searchBarBorderHeight = panel.getBarBorderHeight();

        final int iconColor = panel.getIconColor();
        final int dragHandlebarColor = panel.getDragHandlebarColor();
        final @ColorInt int progressBarBackgroundColor = panel.getProgressBarBackgroundColor();
        final @ColorInt int progressBarColor = panel.getProgressBarColor();

        float closeIconOpacity = panel.getCloseIconOpacity();

        boolean isProgressBarVisible = panel.isProgressBarVisible();

        float progressBarHeight = panel.getProgressBarHeight();
        float progressBarOpacity = panel.getProgressBarOpacity();
        float progressBarCompletion = panel.getProgressBarCompletion();

        boolean touchHighlightVisible = searchBarControl.getTouchHighlightVisible();
        float touchHighlightXOffset = searchBarControl.getTouchHighlightXOffsetPx();
        float touchHighlightWidth = searchBarControl.getTouchHighlightWidthPx();

        WebContents panelWebContents = panel.getWebContents();

        int roundedBarTopResourceId = R.drawable.top_round_foreground;
        int separatorLineColor = panel.getSeparatorLineColor();
        int panelShadowResourceId = R.drawable.top_round_shadow;
        int closeIconResourceId = INVALID_RESOURCE_ID;

        // TODO(donnd): crbug.com/1143472 - Remove parameters for the now
        // defunct close button from the interface and the associated code on
        // the native side.
        ContextualSearchSceneLayerJni.get()
                .updateContextualSearchLayer(
                        mNativePtr,
                        panelShadowResourceId,
                        searchBarBackgroundColor,
                        searchContextViewId,
                        searchTermViewId,
                        searchCaptionViewId,
                        R.drawable.modern_toolbar_shadow,
                        R.drawable.ic_logo_googleg_24dp,
                        quickActionIconResId,
                        dragHandlebarId,
                        openNewTabIconId,
                        closeIconResourceId,
                        R.drawable.progress_bar_background,
                        progressBarBackgroundColor,
                        R.drawable.progress_bar_foreground,
                        progressBarColor,
                        searchPromoViewId,
                        mDpToPx,
                        panel.getFullscreenWidth() * mDpToPx,
                        panel.getTabHeight() * mDpToPx,
                        panel.getBasePageBrightness(),
                        panel.getBasePageY() * mDpToPx,
                        panelWebContents,
                        searchPromoVisible,
                        searchPromoHeightPx,
                        searchPromoOpacity,
                        searchPromoBackgroundColor,
                        // Related Searches
                        relatedSearchesInBarViewId,
                        relatedSearchesInBarVisible,
                        relatedSearchesInBarHeight,
                        relatedSearchesInBarRedundantPadding,
                        // Panel position etc.
                        searchPanelX * mDpToPx,
                        searchPanelY * mDpToPx,
                        searchPanelWidth * mDpToPx,
                        searchPanelHeight * mDpToPx,
                        searchBarMarginSide * mDpToPx,
                        searchBarMarginTop * mDpToPx,
                        searchBarMarginBottom,
                        searchBarHeight * mDpToPx,
                        searchContextOpacity,
                        searchBarControl.getTextLayerMinHeight(),
                        searchTermOpacity,
                        searchBarControl.getSearchTermCaptionSpacing(),
                        searchCaptionAnimationPercentage,
                        searchCaptionVisible,
                        searchBarBorderVisible,
                        searchBarBorderHeight * mDpToPx,
                        quickActionIconVisible,
                        thumbnailVisible,
                        thumbnailUrl,
                        customImageVisibilityPercentage,
                        barImageSize,
                        iconColor,
                        dragHandlebarColor,
                        closeIconOpacity,
                        isProgressBarVisible,
                        progressBarHeight * mDpToPx,
                        progressBarOpacity,
                        progressBarCompletion,
                        touchHighlightVisible,
                        touchHighlightXOffset,
                        touchHighlightWidth,
                        mProfile,
                        roundedBarTopResourceId,
                        separatorLineColor);
    }

    @CalledByNative
    public void onThumbnailFetched(boolean success) {
        if (mImageControl != null) mImageControl.onThumbnailFetched(success);
    }

    @Override
    public void setContentTree(SceneLayer contentTree) {
        ContextualSearchSceneLayerJni.get().setContentTree(mNativePtr, contentTree);
    }

    /** Hide the layer tree; for use if the panel is not being shown. */
    public void hideTree() {
        if (!mIsInitialized) return;
        ContextualSearchSceneLayerJni.get().hideTree(mNativePtr);
    }

    @Override
    protected void initializeNative() {
        if (mNativePtr == 0) {
            mNativePtr = ContextualSearchSceneLayerJni.get().init(ContextualSearchSceneLayer.this);
        }
        assert mNativePtr != 0;
    }

    /** Destroys this object and the corresponding native component. */
    @Override
    public void destroy() {
        super.destroy();
        mIsInitialized = false;
        mNativePtr = 0;
    }

    @NativeMethods
    interface Natives {
        long init(ContextualSearchSceneLayer caller);

        void createContextualSearchLayer(
                long nativeContextualSearchSceneLayer, ResourceManager resourceManager);

        void setContentTree(long nativeContextualSearchSceneLayer, SceneLayer contentTree);

        void hideTree(long nativeContextualSearchSceneLayer);

        void updateContextualSearchLayer(
                long nativeContextualSearchSceneLayer,
                int searchBarBackgroundResourceId,
                int searchBarBackgroundColor,
                int searchContextResourceId,
                int searchTermResourceId,
                int searchCaptionResourceId,
                int searchBarShadowResourceId,
                int searchProviderIconResourceId,
                int quickActionIconResourceId,
                int dragHandlebarResourceId,
                int openTabIconResourceId,
                int closeIconResourceId,
                int progressBarBackgroundResourceId,
                int progressBarBackgroundColor,
                int progressBarResourceId,
                int progressBarColor,
                int searchPromoResourceId,
                float dpToPx,
                float layoutWidth,
                float layoutHeight,
                float basePageBrightness,
                float basePageYOffset,
                @JniType("content::WebContents*") WebContents webContents,
                boolean searchPromoVisible,
                float searchPromoHeight,
                float searchPromoOpacity,
                int searchPromoBackgroundColor,
                // Related Searches
                int relatedSearchesInBarResourceId,
                boolean relatedSearchesInBarVisible,
                float relatedSearchesInBarHeight,
                float relatedSearchesInBarRedundantPadding,
                // Panel position etc
                float searchPanelX,
                float searchPanelY,
                float searchPanelWidth,
                float searchPanelHeight,
                float searchBarMarginSide,
                float searchBarMarginTop,
                float searchBarMarginBottom,
                float searchBarHeight,
                float searchContextOpacity,
                float searchTextLayerMinHeight,
                float searchTermOpacity,
                float searchTermCaptionSpacing,
                float searchCaptionAnimationPercentage,
                boolean searchCaptionVisible,
                boolean searchBarBorderVisible,
                float searchBarBorderHeight,
                boolean quickActionIconVisible,
                boolean thumbnailVisible,
                @JniType("std::string") String thumbnailUrl,
                float customImageVisibilityPercentage,
                int barImageSize,
                int iconColor,
                int dragHandlebarColor,
                float closeIconOpacity,
                boolean isProgressBarVisible,
                float progressBarHeight,
                float progressBarOpacity,
                float progressBarCompletion,
                boolean touchHighlightVisible,
                float touchHighlightXOffset,
                float toucHighlightWidth,
                @JniType("Profile*") Profile profile,
                int barBackgroundResourceId,
                int separatorLineColor);
    }
}
