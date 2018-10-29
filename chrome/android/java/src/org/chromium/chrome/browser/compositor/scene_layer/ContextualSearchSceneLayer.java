// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.scene_layer;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.bottombar.contextualsearch.ContextualSearchBarBannerControl;
import org.chromium.chrome.browser.compositor.bottombar.contextualsearch.ContextualSearchBarControl;
import org.chromium.chrome.browser.compositor.bottombar.contextualsearch.ContextualSearchImageControl;
import org.chromium.chrome.browser.compositor.bottombar.contextualsearch.ContextualSearchPanel;
import org.chromium.chrome.browser.compositor.bottombar.contextualsearch.ContextualSearchPromoControl;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.resources.ResourceManager;

/**
 * A SceneLayer to render layers for ContextualSearchLayout.
 */
@JNINamespace("android")
public class ContextualSearchSceneLayer extends SceneOverlayLayer {
    // NOTE: If you use SceneLayer's native pointer here, the JNI generator will try to
    // downcast using reinterpret_cast<>. We keep a separate pointer to avoid it.
    private long mNativePtr;

    /** If the scene layer has been initialized. */
    private boolean mIsInitialized;

    private final float mDpToPx;

    private ContextualSearchImageControl mImageControl;

    public ContextualSearchSceneLayer(float dpToPx) {
        mDpToPx = dpToPx;
    }

    /**
     * Update the scene layer to draw an OverlayPanel.
     * @param resourceManager Manager to get view and image resources.
     * @param panel The OverlayPanel to render.
     * @param searchBarControl The Search Bar control.
     * @param barBannerControl The promotion for Contextual Search.
     * @param imageControl The object controlling the image displayed in the Bar.
     */
    public void update(ResourceManager resourceManager, ContextualSearchPanel panel,
            ContextualSearchBarControl searchBarControl,
            ContextualSearchBarBannerControl barBannerControl,
            ContextualSearchPromoControl promoControl, ContextualSearchImageControl imageControl) {
        // Don't try to update the layer if not initialized or showing.
        if (resourceManager == null || !panel.isShowing()) return;

        if (!mIsInitialized) {
            nativeCreateContextualSearchLayer(mNativePtr, resourceManager);
            mIsInitialized = true;
        }
        mImageControl = imageControl;

        int searchContextViewId = searchBarControl.getSearchContextViewId();
        int searchTermViewId = searchBarControl.getSearchTermViewId();
        int searchCaptionViewId = searchBarControl.getCaptionViewId();

        int searchPromoViewId = promoControl.getViewId();
        boolean searchPromoVisible = promoControl.isVisible();
        float searchPromoHeightPx = promoControl.getHeightPx();
        float searchPromoOpacity = promoControl.getOpacity();

        int searchBarBannerTextViewId = barBannerControl.getViewId();
        boolean searchBarBannerVisible = barBannerControl.isVisible();
        float searchBarBannerHeightPx = barBannerControl.getHeightPx();
        float searchBarBannerPaddingPx = barBannerControl.getPaddingPx();
        float searchBarBannerRippleWidthPx = barBannerControl.getRippleWidthPx();
        float searchBarBannerRippleOpacity = barBannerControl.getRippleOpacity();
        float searchBarBannerTextOpacity = barBannerControl.getTextOpacity();

        float customImageVisibilityPercentage = imageControl.getCustomImageVisibilityPercentage();
        int barImageSize = imageControl.getBarImageSize();

        boolean quickActionIconVisible = imageControl.getQuickActionIconVisible();
        int quickActionIconResId = imageControl.getQuickActionIconResourceId();

        boolean thumbnailVisible = imageControl.getThumbnailVisible();
        String thumbnailUrl = imageControl.getThumbnailUrl();

        float searchPanelX = panel.getOffsetX();
        float searchPanelY = panel.getOffsetY();
        float searchPanelWidth = panel.getWidth();
        float searchPanelHeight = panel.getHeight();

        float searchBarMarginSide = panel.getBarMarginSide();
        float searchBarHeight = panel.getBarHeight();

        float searchContextOpacity = searchBarControl.getSearchBarContextOpacity();
        float searchTermOpacity = searchBarControl.getSearchBarTermOpacity();

        float searchCaptionAnimationPercentage = searchBarControl.getCaptionAnimationPercentage();
        boolean searchCaptionVisible = searchBarControl.getCaptionVisible();

        boolean searchBarBorderVisible = panel.isBarBorderVisible();
        float searchBarBorderHeight = panel.getBarBorderHeight();

        boolean searchBarShadowVisible = panel.getBarShadowVisible();
        float searchBarShadowOpacity = panel.getBarShadowOpacity();

        float arrowIconOpacity = panel.getArrowIconOpacity();
        float arrowIconRotation = panel.getArrowIconRotation();

        float closeIconOpacity = panel.getCloseIconOpacity();

        boolean isProgressBarVisible = panel.isProgressBarVisible();

        float progressBarHeight = panel.getProgressBarHeight();
        float progressBarOpacity = panel.getProgressBarOpacity();
        int progressBarCompletion = panel.getProgressBarCompletion();

        float dividerLineVisibilityPercentage =
                searchBarControl.getDividerLineVisibilityPercentage();
        float dividerLineWidth = searchBarControl.getDividerLineWidth();
        float dividerLineHeight = searchBarControl.getDividerLineHeight();
        int dividerLineColor = searchBarControl.getDividerLineColor();
        float dividerLineXOffset = searchBarControl.getDividerLineXOffset();

        boolean touchHighlightVisible = searchBarControl.getTouchHighlightVisible();
        float touchHighlightXOffset = searchBarControl.getTouchHighlightXOffsetPx();
        float touchHighlightWidth = searchBarControl.getTouchHighlightWidthPx();

        WebContents panelWebContents = panel.getWebContents();

        nativeUpdateContextualSearchLayer(mNativePtr, R.drawable.contextual_search_bar_background,
                searchContextViewId, searchTermViewId, searchCaptionViewId,
                R.drawable.modern_toolbar_shadow, R.drawable.ic_logo_googleg_24dp,
                quickActionIconResId, R.drawable.breadcrumb_arrow,
                ContextualSearchPanel.CLOSE_ICON_DRAWABLE_ID, R.drawable.progress_bar_background,
                R.drawable.progress_bar_foreground, searchPromoViewId,
                R.drawable.contextual_search_promo_ripple, searchBarBannerTextViewId, mDpToPx,
                panel.getFullscreenWidth() * mDpToPx, panel.getTabHeight() * mDpToPx,
                panel.getBasePageBrightness(), panel.getBasePageY() * mDpToPx, panelWebContents,
                searchPromoVisible, searchPromoHeightPx, searchPromoOpacity, searchBarBannerVisible,
                searchBarBannerHeightPx, searchBarBannerPaddingPx, searchBarBannerRippleWidthPx,
                searchBarBannerRippleOpacity, searchBarBannerTextOpacity, searchPanelX * mDpToPx,
                searchPanelY * mDpToPx, searchPanelWidth * mDpToPx, searchPanelHeight * mDpToPx,
                searchBarMarginSide * mDpToPx, searchBarHeight * mDpToPx, searchContextOpacity,
                searchBarControl.getTextLayerMinHeight(), searchTermOpacity,
                searchBarControl.getSearchTermCaptionSpacing(), searchCaptionAnimationPercentage,
                searchCaptionVisible, searchBarBorderVisible, searchBarBorderHeight * mDpToPx,
                searchBarShadowVisible, searchBarShadowOpacity, quickActionIconVisible,
                thumbnailVisible, thumbnailUrl, customImageVisibilityPercentage, barImageSize,
                arrowIconOpacity, arrowIconRotation, closeIconOpacity, isProgressBarVisible,
                progressBarHeight * mDpToPx, progressBarOpacity, progressBarCompletion,
                dividerLineVisibilityPercentage, dividerLineWidth, dividerLineHeight,
                dividerLineColor, dividerLineXOffset, touchHighlightVisible, touchHighlightXOffset,
                touchHighlightWidth, Profile.getLastUsedProfile());
    }

    @CalledByNative
    public void onThumbnailFetched(boolean success) {
        if (mImageControl != null) mImageControl.onThumbnailFetched(success);
    }

    @Override
    public void setContentTree(SceneLayer contentTree) {
        nativeSetContentTree(mNativePtr, contentTree);
    }

    /**
     * Hide the layer tree; for use if the panel is not being shown.
     */
    public void hideTree() {
        if (!mIsInitialized) return;
        nativeHideTree(mNativePtr);
    }

    @Override
    protected void initializeNative() {
        if (mNativePtr == 0) {
            mNativePtr = nativeInit();
        }
        assert mNativePtr != 0;
    }

    /**
     * Destroys this object and the corresponding native component.
     */
    @Override
    public void destroy() {
        super.destroy();
        mIsInitialized = false;
        mNativePtr = 0;
    }

    private native long nativeInit();
    private native void nativeCreateContextualSearchLayer(
            long nativeContextualSearchSceneLayer,
            ResourceManager resourceManager);
    private native void nativeSetContentTree(
            long nativeContextualSearchSceneLayer,
            SceneLayer contentTree);
    private native void nativeHideTree(
            long nativeContextualSearchSceneLayer);
    private native void nativeUpdateContextualSearchLayer(long nativeContextualSearchSceneLayer,
            int searchBarBackgroundResourceId, int searchContextResourceId,
            int searchTermResourceId, int searchCaptionResourceId, int searchBarShadowResourceId,
            int searchProviderIconResourceId, int quickActionIconResourceId, int arrowUpResourceId,
            int closeIconResourceId, int progressBarBackgroundResourceId, int progressBarResourceId,
            int searchPromoResourceId, int barBannerRippleResourceId, int barBannerTextResourceId,
            float dpToPx, float layoutWidth, float layoutHeight, float basePageBrightness,
            float basePageYOffset, WebContents webContents, boolean searchPromoVisible,
            float searchPromoHeight, float searchPromoOpacity, boolean searchBarBannerVisible,
            float searchBarBannerHeight, float searchBarBannerPaddingPx,
            float searchBarBannerRippleWidth, float searchBarBannerRippleOpacity,
            float searchBarBannerTextOpacity, float searchPanelX, float searchPanelY,
            float searchPanelWidth, float searchPanelHeight, float searchBarMarginSide,
            float searchBarHeight, float searchContextOpacity, float searchTextLayerMinHeight,
            float searchTermOpacity, float searchTermCaptionSpacing,
            float searchCaptionAnimationPercentage, boolean searchCaptionVisible,
            boolean searchBarBorderVisible, float searchBarBorderHeight,
            boolean searchBarShadowVisible, float searchBarShadowOpacity,
            boolean quickActionIconVisible, boolean thumbnailVisible, String thumbnailUrl,
            float customImageVisibilityPercentage, int barImageSize, float arrowIconOpacity,
            float arrowIconRotation, float closeIconOpacity, boolean isProgressBarVisible,
            float progressBarHeight, float progressBarOpacity, int progressBarCompletion,
            float dividerLineVisibilityPercentage, float dividerLineWidth, float dividerLineHeight,
            int dividerLineColor, float dividerLineXOffset, boolean touchHighlightVisible,
            float touchHighlightXOffset, float toucHighlightWidth, Profile profile);
}
