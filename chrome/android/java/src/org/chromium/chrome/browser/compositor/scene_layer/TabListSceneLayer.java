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
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.compositor.LayerTitleCache;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.compositor.layouts.components.LayoutTab;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.layouts.scene_layer.SceneLayer;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_management.TabUiFeatureUtilities;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.ui.resources.ResourceManager;
import org.chromium.ui.util.ColorUtils;

/**
 * A SceneLayer to render a tab stack.
 * TODO(changwan): change layouts to share one instance of this.
 */
@JNINamespace("android")
public class TabListSceneLayer extends SceneLayer {
    private long mNativePtr;
    private TabModelSelector mTabModelSelector;
    private int[] mAdditionalIds = new int[4];
    private boolean mUseAdditionalIds;
    private boolean mIsInitialized;

    public void setTabModelSelector(TabModelSelector tabModelSelector) {
        mTabModelSelector = tabModelSelector;
    }

    public void init(LayerTitleCache layerTitleCache, TabContentManager tabContentManager,
            ResourceManager resourceManager) {
        if (mNativePtr == 0 || mIsInitialized) return;
        TabListSceneLayerJni.get().setDependencies(mNativePtr, TabListSceneLayer.this,
                tabContentManager, layerTitleCache, resourceManager);
        mIsInitialized = true;
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
     * @param browserControls The provider for browser controls state.
     * @param backgroundResourceId The resource ID for background. {@link #INVALID_RESOURCE_ID} if
     *                             none. Only used in GridTabSwitcher.
     * @param backgroundAlpha The alpha of the background. Only used in GridTabSwitcher.
     * @param backgroundTopOffset The top offset of the background. Only used in GridTabSwitcher.
     *
     */
    public void pushLayers(Context context, RectF viewport, RectF contentViewport, Layout layout,
            LayerTitleCache layerTitleCache, TabContentManager tabContentManager,
            ResourceManager resourceManager, BrowserControlsStateProvider browserControls,
            int backgroundResourceId, float backgroundAlpha, int backgroundTopOffset) {
        if (mNativePtr == 0) return;

        Resources res = context.getResources();
        final float dpToPx = res.getDisplayMetrics().density;
        final int tabListBgColor = getTabListBackgroundColor(context);

        LayoutTab[] tabs = layout.getLayoutTabsToRender();
        int tabsCount = tabs != null ? tabs.length : 0;

        if (!mIsInitialized) {
            init(layerTitleCache, tabContentManager, resourceManager);
        }

        TabListSceneLayerJni.get().beginBuildingFrame(mNativePtr, TabListSceneLayer.this);

        // TODO(crbug.com/1070281): Use Supplier to get viewport and forward it to native, then
        // updateLayer can become obsolete.
        TabListSceneLayerJni.get().updateLayer(mNativePtr, TabListSceneLayer.this, tabListBgColor,
                viewport.left, viewport.top, viewport.width(), viewport.height());

        if (backgroundResourceId != INVALID_RESOURCE_ID) {
            TabListSceneLayerJni.get().putBackgroundLayer(mNativePtr, TabListSceneLayer.this,
                    backgroundResourceId, backgroundAlpha, backgroundTopOffset);
        }

        final float shadowAlpha = ColorUtils.shouldUseLightForegroundOnBackground(tabListBgColor)
                ? LayoutTab.SHADOW_ALPHA_ON_DARK_BG
                : LayoutTab.SHADOW_ALPHA_ON_LIGHT_BG;

        for (int i = 0; i < tabsCount; i++) {
            LayoutTab t = tabs[i];
            assert t.isVisible() : "LayoutTab in that list should be visible";
            final float decoration = t.getDecorationAlpha();

            int urlBarBackgroundId = R.drawable.modern_location_bar;
            boolean useIncognitoColors = t.isIncognito();

            int defaultThemeColor = ChromeColors.getDefaultThemeColor(res, useIncognitoColors);

            int closeButtonColor = useIncognitoColors
                    ? Color.WHITE
                    : ApiCompatibilityUtils.getColor(res, R.color.default_icon_color_secondary);
            float closeButtonSizePx =
                    res.getDimensionPixelSize(R.dimen.tab_switcher_close_button_size);

            int borderColorResource =
                    t.isIncognito() ? R.color.tab_back_incognito : R.color.tab_back;

            int[] relatedTabIds = getRelatedTabIds(t.getId());

            float toolbarYOffset = browserControls.getTopControlOffset()
                    + browserControls.getTopControlsMinHeight();

            // TODO(dtrainor, clholgat): remove "* dpToPx" once the native part fully supports dp.
            TabListSceneLayerJni.get().putTabLayer(mNativePtr, TabListSceneLayer.this, t.getId(),
                    relatedTabIds, mUseAdditionalIds, R.id.control_container,
                    R.drawable.btn_delete_24dp, R.drawable.tabswitcher_border_frame_shadow,
                    R.drawable.tabswitcher_border_frame_decoration, R.drawable.logo_card_back,
                    R.drawable.tabswitcher_border_frame,
                    R.drawable.tabswitcher_border_frame_inner_shadow, t.canUseLiveTexture(),
                    t.getBackgroundColor(),
                    ApiCompatibilityUtils.getColor(res, borderColorResource), t.isIncognito(),
                    t.isCloseButtonOnRight(), t.getRenderX() * dpToPx, t.getRenderY() * dpToPx,
                    t.getScaledContentWidth() * dpToPx, t.getScaledContentHeight() * dpToPx,
                    t.getOriginalContentWidth() * dpToPx, t.getOriginalContentHeight() * dpToPx,
                    t.getClippedX() * dpToPx, t.getClippedY() * dpToPx,
                    Math.min(t.getClippedWidth(), t.getScaledContentWidth()) * dpToPx,
                    Math.min(t.getClippedHeight(), t.getScaledContentHeight()) * dpToPx,
                    t.getTiltXPivotOffset() * dpToPx, t.getTiltYPivotOffset() * dpToPx,
                    t.getTiltX(), t.getTiltY(), t.getAlpha(), t.getBorderAlpha() * decoration,
                    t.getBorderInnerShadowAlpha() * decoration, decoration,
                    shadowAlpha * decoration, t.getBorderCloseButtonAlpha() * decoration,
                    LayoutTab.CLOSE_BUTTON_WIDTH_DP * dpToPx, closeButtonSizePx,
                    t.getStaticToViewBlend(), t.getBorderScale(), t.getSaturation(),
                    t.getBrightness(), t.showToolbar(), defaultThemeColor,
                    t.getToolbarBackgroundColor(), closeButtonColor, t.anonymizeToolbar(),
                    t.isTitleNeeded(), urlBarBackgroundId, t.getTextBoxBackgroundColor(),
                    t.getToolbarAlpha(), toolbarYOffset, browserControls.getContentOffset(),
                    t.getSideBorderScale(), t.insetBorderVertical());
        }
        TabListSceneLayerJni.get().finishBuildingFrame(mNativePtr, TabListSceneLayer.this);
    }

    /**
     * @return The background color of the scene layer.
     */
    protected int getTabListBackgroundColor(Context context) {
        int colorId = R.color.default_bg_color;

        if (TabUiFeatureUtilities.isGridTabSwitcherEnabled()) {
            if (mTabModelSelector != null && mTabModelSelector.isIncognitoSelected()) {
                colorId = R.color.default_bg_color_dark;
            } else {
                colorId = R.color.default_bg_color;
            }
        }

        return ApiCompatibilityUtils.getColor(context.getResources(), colorId);
    }

    @Override
    protected void initializeNative() {
        if (mNativePtr == 0) {
            mNativePtr = TabListSceneLayerJni.get().init(TabListSceneLayer.this);
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

    private int[] getRelatedTabIds(int id) {
        // TODO(meiliang): return four tab ids, include the provided id and id of three other
        // closest tabs. These ids comes from TabModelFilter#getUnimodifiableRelatedTabList(int) and
        // update the mAdditionalIds, and mUseAdditionalIds is false when there's no such id.
        mUseAdditionalIds = false;
        return mAdditionalIds;
    }

    @NativeMethods
    interface Natives {
        long init(TabListSceneLayer caller);
        void beginBuildingFrame(long nativeTabListSceneLayer, TabListSceneLayer caller);
        void finishBuildingFrame(long nativeTabListSceneLayer, TabListSceneLayer caller);
        void setDependencies(long nativeTabListSceneLayer, TabListSceneLayer caller,
                TabContentManager tabContentManager, LayerTitleCache layerTitleCache,
                ResourceManager resourceManager);
        void updateLayer(long nativeTabListSceneLayer, TabListSceneLayer caller,
                int backgroundColor, float viewportX, float viewportY, float viewportWidth,
                float viewportHeight);
        // TODO(meiliang): Need to provide a resource that indicates the selected tab on the layer.
        void putTabLayer(long nativeTabListSceneLayer, TabListSceneLayer caller, int selectedId,
                int[] ids, boolean useAdditionalIds, int toolbarResourceId,
                int closeButtonResourceId, int shadowResourceId, int contourResourceId,
                int backLogoResourceId, int borderResourceId, int borderInnerShadowResourceId,
                boolean canUseLiveLayer, int tabBackgroundColor, int backLogoColor,
                boolean incognito, boolean isPortrait, float x, float y, float width, float height,
                float contentWidth, float contentHeight, float shadowX, float shadowY,
                float shadowWidth, float shadowHeight, float pivotX, float pivotY, float rotationX,
                float rotationY, float alpha, float borderAlpha, float borderInnerShadowAlpha,
                float contourAlpha, float shadowAlpha, float closeAlpha, float closeBtnWidth,
                float closeBtnAssetSize, float staticToViewBlend, float borderScale,
                float saturation, float brightness, boolean showToolbar, int defaultThemeColor,
                int toolbarBackgroundColor, int closeButtonColor, boolean anonymizeToolbar,
                boolean showTabTitle, int toolbarTextBoxResource, int toolbarTextBoxBackgroundColor,
                float toolbarTextBoxAlpha, float toolbarYOffset, float contentOffset,
                float sideBorderScale, boolean insetVerticalBorder);

        void putBackgroundLayer(long nativeTabListSceneLayer, TabListSceneLayer caller,
                int resourceId, float alpha, int topOffset);
    }
}
