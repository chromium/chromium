// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.scene_layer;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.RectF;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.compositor.layouts.components.LayoutTab;
import org.chromium.chrome.browser.layouts.scene_layer.SceneLayer;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
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
    private boolean mIsInitialized;

    public void setTabModelSelector(TabModelSelector tabModelSelector) {
        mTabModelSelector = tabModelSelector;
    }

    public void init(TabContentManager tabContentManager, ResourceManager resourceManager) {
        if (mNativePtr == 0 || mIsInitialized) return;
        TabListSceneLayerJni.get()
                .setDependencies(
                        mNativePtr, TabListSceneLayer.this, tabContentManager, resourceManager);
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
     * @param tabContentManager An object for accessing tab content.
     * @param resourceManager An object for accessing static and dynamic resources.
     * @param browserControls The provider for browser controls state.
     * @param backgroundResourceId The resource ID for background. {@link #INVALID_RESOURCE_ID} if
     *                             none. Only used in GridTabSwitcher.
     * @param backgroundAlpha The alpha of the background. Only used in GridTabSwitcher.
     * @param backgroundTopOffset The top offset of the background. Only used in GridTabSwitcher.
     *
     */
    public void pushLayers(
            Context context,
            RectF viewport,
            RectF contentViewport,
            Layout layout,
            TabContentManager tabContentManager,
            ResourceManager resourceManager,
            BrowserControlsStateProvider browserControls,
            int backgroundResourceId,
            float backgroundAlpha,
            int backgroundTopOffset) {
        if (mNativePtr == 0) return;

        Resources res = context.getResources();
        final float dpToPx = res.getDisplayMetrics().density;
        final int tabListBgColor = getTabListBackgroundColor(context);

        LayoutTab[] tabs = layout.getLayoutTabsToRender();
        int tabsCount = tabs != null ? tabs.length : 0;

        if (!mIsInitialized) {
            init(tabContentManager, resourceManager);
        }

        TabListSceneLayerJni.get().beginBuildingFrame(mNativePtr, TabListSceneLayer.this);

        // TODO(crbug.com/40126259): Use Supplier to get viewport and forward it to native, then
        // updateLayer can become obsolete.
        TabListSceneLayerJni.get()
                .updateLayer(
                        mNativePtr,
                        TabListSceneLayer.this,
                        tabListBgColor,
                        viewport.left,
                        viewport.top,
                        viewport.width(),
                        viewport.height());

        if (backgroundResourceId != INVALID_RESOURCE_ID) {
            TabListSceneLayerJni.get()
                    .putBackgroundLayer(
                            mNativePtr,
                            TabListSceneLayer.this,
                            backgroundResourceId,
                            backgroundAlpha,
                            backgroundTopOffset);
        }

        final float shadowAlpha =
                ColorUtils.shouldUseLightForegroundOnBackground(tabListBgColor)
                        ? LayoutTab.SHADOW_ALPHA_ON_DARK_BG
                        : LayoutTab.SHADOW_ALPHA_ON_LIGHT_BG;

        int contentOffset = browserControls != null ? browserControls.getContentOffset() : 0;
        final int urlBarBackgroundId = R.drawable.modern_location_bar;

        for (int i = 0; i < tabsCount; i++) {
            LayoutTab t = tabs[i];
            final float decoration = t.getDecorationAlpha();
            boolean useIncognitoColors = t.isIncognito();
            int defaultThemeColor = ChromeColors.getDefaultThemeColor(context, useIncognitoColors);

            // TODO(dtrainor, clholgat): remove "* dpToPx" once the native part fully supports dp.
            TabListSceneLayerJni.get()
                    .putTabLayer(
                            mNativePtr,
                            TabListSceneLayer.this,
                            t.getId(),
                            R.id.control_container,
                            R.drawable.tabswitcher_border_frame_shadow,
                            R.drawable.tabswitcher_border_frame_decoration,
                            R.drawable.tabswitcher_border_frame,
                            R.drawable.tabswitcher_border_frame_inner_shadow,
                            t.canUseLiveTexture(),
                            t.getBackgroundColor(),
                            t.isIncognito(),
                            t.getRenderX() * dpToPx,
                            t.getRenderY() * dpToPx,
                            t.getScaledContentWidth() * dpToPx,
                            t.getScaledContentHeight() * dpToPx,
                            t.getOriginalContentWidth() * dpToPx,
                            t.getOriginalContentHeight() * dpToPx,
                            Math.min(t.getClippedWidth(), t.getScaledContentWidth()) * dpToPx,
                            Math.min(t.getClippedHeight(), t.getScaledContentHeight()) * dpToPx,
                            t.getAlpha(),
                            t.getBorderAlpha() * decoration,
                            t.getBorderInnerShadowAlpha() * decoration,
                            decoration,
                            shadowAlpha * decoration,
                            t.getStaticToViewBlend(),
                            t.getBorderScale(),
                            t.getSaturation(),
                            t.showToolbar(),
                            defaultThemeColor,
                            t.getToolbarBackgroundColor(),
                            t.anonymizeToolbar(),
                            urlBarBackgroundId,
                            t.getTextBoxBackgroundColor(),
                            contentOffset);
        }
        TabListSceneLayerJni.get().finishBuildingFrame(mNativePtr, TabListSceneLayer.this);
    }

    /** Returns the background color of the scene layer. */
    protected int getTabListBackgroundColor(Context context) {
        if (mTabModelSelector != null && mTabModelSelector.isIncognitoSelected()) {
            return context.getColor(R.color.default_bg_color_dark);
        }
        return SemanticColorUtils.getDefaultBgColor(context);
    }

    @Override
    protected void initializeNative() {
        if (mNativePtr == 0) {
            mNativePtr = TabListSceneLayerJni.get().init(TabListSceneLayer.this);
        }
        assert mNativePtr != 0;
    }

    /** Destroys this object and the corresponding native component. */
    @Override
    public void destroy() {
        super.destroy();
        mNativePtr = 0;
    }

    @NativeMethods
    interface Natives {
        long init(TabListSceneLayer caller);

        void beginBuildingFrame(long nativeTabListSceneLayer, TabListSceneLayer caller);

        void finishBuildingFrame(long nativeTabListSceneLayer, TabListSceneLayer caller);

        void setDependencies(
                long nativeTabListSceneLayer,
                TabListSceneLayer caller,
                TabContentManager tabContentManager,
                ResourceManager resourceManager);

        void updateLayer(
                long nativeTabListSceneLayer,
                TabListSceneLayer caller,
                int backgroundColor,
                float viewportX,
                float viewportY,
                float viewportWidth,
                float viewportHeight);

        // TODO(meiliang): Need to provide a resource that indicates the selected tab on the layer.
        void putTabLayer(
                long nativeTabListSceneLayer,
                TabListSceneLayer caller,
                int selectedId,
                int toolbarResourceId,
                int shadowResourceId,
                int contourResourceId,
                int borderResourceId,
                int borderInnerShadowResourceId,
                boolean canUseLiveLayer,
                int tabBackgroundColor,
                boolean incognito,
                float x,
                float y,
                float width,
                float height,
                float contentWidth,
                float contentHeight,
                float shadowWidth,
                float shadowHeight,
                float alpha,
                float borderAlpha,
                float borderInnerShadowAlpha,
                float contourAlpha,
                float shadowAlpha,
                float staticToViewBlend,
                float borderScale,
                float saturation,
                boolean showToolbar,
                int defaultThemeColor,
                int toolbarBackgroundColor,
                boolean anonymizeToolbar,
                int toolbarTextBoxResource,
                int toolbarTextBoxBackgroundColor,
                float contentOffset);

        void putBackgroundLayer(
                long nativeTabListSceneLayer,
                TabListSceneLayer caller,
                int resourceId,
                float alpha,
                int topOffset);
    }
}
