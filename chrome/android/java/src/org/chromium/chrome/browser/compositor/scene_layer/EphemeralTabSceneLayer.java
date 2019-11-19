// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.scene_layer;

import androidx.annotation.Nullable;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel;
import org.chromium.chrome.browser.compositor.bottombar.ephemeraltab.EphemeralTabBarControl;
import org.chromium.chrome.browser.compositor.bottombar.ephemeraltab.EphemeralTabCaptionControl;
import org.chromium.chrome.browser.compositor.bottombar.ephemeraltab.EphemeralTabPanel;
import org.chromium.chrome.browser.compositor.bottombar.ephemeraltab.EphemeralTabTitleControl;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.resources.ResourceManager;

/**
 * A SceneLayer to render layers for Ephemeral Tab.
 */
@JNINamespace("android")
public class EphemeralTabSceneLayer extends SceneOverlayLayer {
    /** Pointer to native EphemeralTabSceneLayer. */
    private long mNativePtr;

    /** If the scene layer has been initialized. */
    private boolean mIsInitialized;

    /** The conversion multiple from dp to px. */
    private final float mDpToPx;

    /** Interface to get notified that favicon is available. */
    interface FaviconCallback {
        /**
         * Called when a favicon becomes available. Used to start the animation fading
         * out the default icon and fading in the favicon.
         */
        @CalledByNative("FaviconCallback")
        void onAvailable();
    }

    /**
     * @param dpToPx The conversion multiple from dp to px for the device.
     * @param faviconSizeDp Preferred size of the favicon to fetch.
     */
    public EphemeralTabSceneLayer(float dpToPx, int faviconSizeDp) {
        mDpToPx = dpToPx;
    }

    /**
     * Update the scene layer to draw an OverlayPanel.
     * @param resourceManager Manager to get view and image resources.
     * @param panel The OverlayPanel to render.
     * @param bar {@link EphemeralTabBarControl} object.
     * @param title {@link EphemeralTabTitleControl} object.
     * @param caption {@link EphemeralTabCaptionControl} object.
     */
    public void update(ResourceManager resourceManager, EphemeralTabPanel panel,
            EphemeralTabBarControl bar, EphemeralTabTitleControl title,
            @Nullable EphemeralTabCaptionControl caption) {
        // Don't try to update the layer if not initialized or showing.
        if (resourceManager == null || !panel.isShowing()) return;
        if (!mIsInitialized) {
            EphemeralTabSceneLayerJni.get().createEphemeralTabLayer(mNativePtr,
                    EphemeralTabSceneLayer.this, resourceManager,
                    () -> panel.startFaviconAnimation(true));
            int openInTabIconId = (OverlayPanel.isNewLayout() && panel.canPromoteToNewTab())
                    ? R.drawable.open_in_new_tab
                    : INVALID_RESOURCE_ID;
            int dragHandlebarId =
                    OverlayPanel.isNewLayout() ? R.drawable.drag_handlebar : INVALID_RESOURCE_ID;
            int roundedBarTopId =
                    OverlayPanel.isNewLayout() ? R.drawable.top_round : INVALID_RESOURCE_ID;
            // The panel shadow goes all the way around in the old layout, but in the new layout
            // the top_round resource also includes the shadow so we only need a side shadow.
            // In either case there's just one shadow-only resource needed.
            int panelShadowResourceId = OverlayPanel.isNewLayout()
                    ? R.drawable.overlay_side_shadow
                    : R.drawable.contextual_search_bar_background;
            EphemeralTabSceneLayerJni.get().setResourceIds(mNativePtr, EphemeralTabSceneLayer.this,
                    title.getViewId(), panelShadowResourceId, roundedBarTopId,
                    R.drawable.modern_toolbar_shadow, R.drawable.infobar_chrome, dragHandlebarId,
                    openInTabIconId, R.drawable.btn_close);
            mIsInitialized = true;
        }

        int titleViewId = title.getViewId();
        int captionViewId = 0;
        int captionIconId = 0;
        float captionIconOpacity = 0.f;
        float captionAnimationPercentage = 0.f;
        boolean captionVisible = false;
        if (caption != null) {
            captionViewId = caption.getViewId();
            captionAnimationPercentage = caption.getAnimationPercentage();
            captionIconOpacity = caption.getIconOpacity();
            captionVisible = caption.getIsVisible();
            captionIconId = caption.getIconId();
        }
        boolean isProgressBarVisible = panel.isProgressBarVisible();
        float progressBarHeight = panel.getProgressBarHeight();
        float progressBarOpacity = panel.getProgressBarOpacity();
        float progressBarCompletion = panel.getProgressBarCompletion();
        int separatorLineColor = panel.getSeparatorLineColor();

        WebContents panelWebContents = panel.getWebContents();
        EphemeralTabSceneLayerJni.get().update(mNativePtr, EphemeralTabSceneLayer.this, titleViewId,
                captionViewId, captionIconId, captionIconOpacity, captionAnimationPercentage,
                bar.getTextLayerMinHeight(), bar.getTitleCaptionSpacing(), captionVisible,
                R.drawable.progress_bar_background, R.drawable.progress_bar_foreground, mDpToPx,
                panel.getBasePageBrightness(), panel.getBasePageY() * mDpToPx, panelWebContents,
                panel.getOffsetX() * mDpToPx, panel.getOffsetY() * mDpToPx,
                panel.getWidth() * mDpToPx, panel.getHeight() * mDpToPx,
                panel.getBarBackgroundColor(), panel.getBarMarginSide() * mDpToPx,
                panel.getBarMarginTop() * mDpToPx, panel.getBarHeight() * mDpToPx,
                panel.isBarBorderVisible(), panel.getBarBorderHeight() * mDpToPx,
                panel.getIconColor(), panel.getDragHandlebarColor(), panel.getFaviconOpacity(),
                isProgressBarVisible, progressBarHeight * mDpToPx, progressBarOpacity,
                progressBarCompletion, separatorLineColor);
    }

    @Override
    public void setContentTree(SceneLayer contentTree) {
        EphemeralTabSceneLayerJni.get().setContentTree(
                mNativePtr, EphemeralTabSceneLayer.this, contentTree);
    }

    /**
     * Hide the layer tree; for use if the panel is not being shown.
     */
    public void hideTree() {
        if (!mIsInitialized) return;
        EphemeralTabSceneLayerJni.get().hideTree(mNativePtr, EphemeralTabSceneLayer.this);
    }

    @Override
    protected void initializeNative() {
        if (mNativePtr == 0) {
            mNativePtr = EphemeralTabSceneLayerJni.get().init(EphemeralTabSceneLayer.this);
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

    @NativeMethods
    interface Natives {
        long init(EphemeralTabSceneLayer caller);
        void createEphemeralTabLayer(long nativeEphemeralTabSceneLayer,
                EphemeralTabSceneLayer caller, ResourceManager resourceManager,
                FaviconCallback callback);
        void setContentTree(long nativeEphemeralTabSceneLayer, EphemeralTabSceneLayer caller,
                SceneLayer contentTree);
        void hideTree(long nativeEphemeralTabSceneLayer, EphemeralTabSceneLayer caller);
        void setResourceIds(long nativeEphemeralTabSceneLayer, EphemeralTabSceneLayer caller,
                int barTextResourceId, int barBackgroundResourceId, int roundedBarTopResourceId,
                int barShadowResourceId, int panelIconResourceId, int dragHandlebarResourceId,
                int openTabIconResourceId, int closeIconResourceId);
        void update(long nativeEphemeralTabSceneLayer, EphemeralTabSceneLayer caller,
                int titleViewId, int captionViewId, int captionIconId, float captionIconOpacity,
                float captionAnimationPercentage, float textLayerMinHeight,
                float titleCaptionSpacing, boolean captionVisible,
                int progressBarBackgroundResourceId, int progressBarResourceId, float dpToPx,
                float basePageBrightness, float basePageYOffset, WebContents webContents,
                float panelX, float panelY, float panelWidth, float panelHeight,
                int barBackgroundColor, float barMarginSide, float barMarginTop, float barHeight,
                boolean barBorderVisible, float barBorderHeight, int iconColor,
                int dragHandlebarColor, float faviconOpacity, boolean isProgressBarVisible,
                float progressBarHeight, float progressBarOpacity, float progressBarCompletion,
                int separatorLineColor);
    }
}
