// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.scene_layer;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Color;
import android.graphics.RectF;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.compositor.LayerTitleCache;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.compositor.layouts.components.LayoutTab;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.util.ColorUtils;
import org.chromium.ui.resources.ResourceManager;

/**
 * A SceneLayer to render a tab stack.
 * TODO(changwan): change layouts to share one instance of this.
 */
@JNINamespace("android")
public class TabListSceneLayer extends SceneLayer {
    private long mNativePtr;
    private TabModelSelector mTabModelSelector;

    public void setTabModelSelector(TabModelSelector tabModelSelector) {
        mTabModelSelector = tabModelSelector;
    }

    /**
     * Pushes all relevant {@link LayoutTab}s from a {@link Layout} to the CC Layer tree.  This will
     * let them be rendered on the screen.  This should only be called when the Compositor has
     * disabled ScheduleComposite calls as this will change the tree and could subsequently cause
     * unnecessary follow up renders.
     * @param context The {@link Context} to use to query device information.
     * @param viewport The viewport for the screen.
     * @param contentViewport The visible viewport.
     * @param layout The {@link Layout} to push to the screen.
     * @param layerTitleCache An object for accessing tab layer titles.
     * @param tabContentManager An object for accessing tab content.
     * @param resourceManager An object for accessing static and dynamic resources.
     * @param fullscreenManager The fullscreen manager for browser controls information.
     */
    public void pushLayers(Context context, RectF viewport, RectF contentViewport, Layout layout,
            LayerTitleCache layerTitleCache, TabContentManager tabContentManager,
            ResourceManager resourceManager, ChromeFullscreenManager fullscreenManager) {
        if (mNativePtr == 0) return;

        Resources res = context.getResources();
        final float dpToPx = res.getDisplayMetrics().density;

        LayoutTab[] tabs = layout.getLayoutTabsToRender();
        int tabsCount = tabs != null ? tabs.length : 0;

        nativeBeginBuildingFrame(mNativePtr);

        nativeUpdateLayer(mNativePtr, getTabListBackgroundColor(context), viewport.left,
                viewport.top, viewport.width(), viewport.height(), layerTitleCache,
                tabContentManager, resourceManager);

        boolean isHTSEnabled =
                ChromeFeatureList.isEnabled(ChromeFeatureList.HORIZONTAL_TAB_SWITCHER_ANDROID);

        for (int i = 0; i < tabsCount; i++) {
            LayoutTab t = tabs[i];
            assert t.isVisible() : "LayoutTab in that list should be visible";
            final float decoration = t.getDecorationAlpha();

            float shadowAlpha = decoration / 2;
            int urlBarBackgroundId = R.drawable.modern_location_bar;
            boolean useLightIcon = t.isIncognito() && !isHTSEnabled;

            int defaultThemeColor = ColorUtils.getDefaultThemeColor(res, useLightIcon);

            // In the modern design, the text box is always drawn opaque in the compositor.
            float textBoxAlpha = 1.f;

            int closeButtonColor = useLightIcon
                    ? Color.WHITE
                    : ApiCompatibilityUtils.getColor(res, R.color.light_icon_color);
            float closeButtonSizePx =
                    res.getDimensionPixelSize(R.dimen.tab_switcher_close_button_size);

            int borderColorResource =
                    t.isIncognito() ? R.color.tab_back_incognito : R.color.tab_back;
            // TODO(dtrainor, clholgat): remove "* dpToPx" once the native part fully supports dp.
            nativePutTabLayer(mNativePtr, t.getId(), R.id.control_container,
                    R.drawable.btn_delete_24dp, R.drawable.tabswitcher_border_frame_shadow,
                    R.drawable.tabswitcher_border_frame_decoration, R.drawable.logo_card_back,
                    R.drawable.tabswitcher_border_frame,
                    R.drawable.tabswitcher_border_frame_inner_shadow, t.canUseLiveTexture(),
                    t.getBackgroundColor(),
                    ApiCompatibilityUtils.getColor(res, borderColorResource), t.isIncognito(),
                    t.isCloseButtonOnRight(), t.getRenderX() * dpToPx, t.getRenderY() * dpToPx,
                    t.getScaledContentWidth() * dpToPx, t.getScaledContentHeight() * dpToPx,
                    t.getOriginalContentWidth() * dpToPx, t.getOriginalContentHeight() * dpToPx,
                    contentViewport.height(), t.getClippedX() * dpToPx, t.getClippedY() * dpToPx,
                    Math.min(t.getClippedWidth(), t.getScaledContentWidth()) * dpToPx,
                    Math.min(t.getClippedHeight(), t.getScaledContentHeight()) * dpToPx,
                    t.getTiltXPivotOffset() * dpToPx, t.getTiltYPivotOffset() * dpToPx,
                    t.getTiltX(), t.getTiltY(), t.getAlpha(), t.getBorderAlpha() * decoration,
                    t.getBorderInnerShadowAlpha() * decoration, decoration, shadowAlpha,
                    t.getBorderCloseButtonAlpha() * decoration,
                    LayoutTab.CLOSE_BUTTON_WIDTH_DP * dpToPx, closeButtonSizePx,
                    t.getStaticToViewBlend(), t.getBorderScale(), t.getSaturation(),
                    t.getBrightness(), t.showToolbar(), defaultThemeColor,
                    t.getToolbarBackgroundColor(), closeButtonColor, t.anonymizeToolbar(),
                    t.isTitleNeeded(), urlBarBackgroundId, t.getTextBoxBackgroundColor(),
                    textBoxAlpha, t.getToolbarAlpha(), t.getToolbarYOffset() * dpToPx,
                    t.getSideBorderScale(), t.insetBorderVertical());
        }
        nativeFinishBuildingFrame(mNativePtr);
    }

    /**
     * @return The background color of the scene layer.
     */
    protected int getTabListBackgroundColor(Context context) {
        int colorId = R.color.modern_primary_color;

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.HORIZONTAL_TAB_SWITCHER_ANDROID)) {
            if (mTabModelSelector != null && mTabModelSelector.isIncognitoSelected()) {
                colorId = R.color.incognito_modern_primary_color;
            } else {
                colorId = R.color.modern_primary_color;
            }
        }

        return ApiCompatibilityUtils.getColor(context.getResources(), colorId);
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
        mNativePtr = 0;
    }

    private native long nativeInit();

    private native void nativeBeginBuildingFrame(long nativeTabListSceneLayer);

    private native void nativeFinishBuildingFrame(long nativeTabListSceneLayer);

    private native void nativeUpdateLayer(long nativeTabListSceneLayer, int backgroundColor,
            float viewportX, float viewportY, float viewportWidth, float viewportHeight,
            LayerTitleCache layerTitleCache, TabContentManager tabContentManager,
            ResourceManager resourceManager);

    private native void nativePutTabLayer(long nativeTabListSceneLayer, int id,
            int toolbarResourceId, int closeButtonResourceId, int shadowResourceId,
            int contourResourceId, int backLogoResourceId, int borderResourceId,
            int borderInnerShadowResourceId, boolean canUseLiveLayer, int tabBackgroundColor,
            int backLogoColor, boolean incognito, boolean isPortrait, float x, float y, float width,
            float height, float contentWidth, float contentHeight, float visibleContentHeight,
            float shadowX, float shadowY, float shadowWidth, float shadowHeight, float pivotX,
            float pivotY, float rotationX, float rotationY, float alpha, float borderAlpha,
            float borderInnerShadowAlpha, float contourAlpha, float shadowAlpha, float closeAlpha,
            float closeBtnWidth, float closeBtnAssetSize, float staticToViewBlend,
            float borderScale, float saturation, float brightness, boolean showToolbar,
            int defaultThemeColor, int toolbarBackgroundColor, int closeButtonColor,
            boolean anonymizeToolbar, boolean showTabTitle, int toolbarTextBoxResource,
            int toolbarTextBoxBackgroundColor, float toolbarTextBoxAlpha, float toolbarAlpha,
            float toolbarYOffset, float sideBorderScale, boolean insetVerticalBorder);
}
