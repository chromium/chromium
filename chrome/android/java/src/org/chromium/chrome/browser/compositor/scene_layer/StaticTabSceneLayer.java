// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.scene_layer;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.compositor.LayerTitleCache;
import org.chromium.chrome.browser.compositor.layouts.components.LayoutTab;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager;

/**
 * A SceneLayer to render a static tab.
 */
@JNINamespace("android")
public class StaticTabSceneLayer extends SceneLayer {
    // NOTE: If you use SceneLayer's native pointer here, the JNI generator will try to
    // downcast using reinterpret_cast<>. We keep a separate pointer to avoid it.
    private long mNativePtr;

    /**
     * Update {@link StaticTabSceneLayer} with the given parameters.
     *
     * @param dpToPx            The ratio of dp to px.
     * @param contentViewport   The viewport of the content.
     * @param layerTitleCache   The LayerTitleCache.
     * @param tabContentManager The TabContentManager.
     * @param fullscreenManager The FullscreenManager.
     * @param layoutTab         The LayoutTab.
     */
    public void update(float dpToPx, LayerTitleCache layerTitleCache,
            TabContentManager tabContentManager, ChromeFullscreenManager fullscreenManager,
            LayoutTab layoutTab) {
        if (layoutTab == null) {
            return;
        }

        float contentOffset =
                fullscreenManager != null ? fullscreenManager.getContentOffset() : 0.f;
        float x = layoutTab.getRenderX() * dpToPx;
        float y = contentOffset + layoutTab.getRenderY() * dpToPx;

        StaticTabSceneLayerJni.get().updateTabLayer(mNativePtr, StaticTabSceneLayer.this,
                tabContentManager, layoutTab.getId(), layoutTab.canUseLiveTexture(),
                layoutTab.getBackgroundColor(), x, y, layoutTab.getStaticToViewBlend(),
                layoutTab.getSaturation(), layoutTab.getBrightness());
    }

    @Override
    protected void initializeNative() {
        if (mNativePtr == 0) {
            mNativePtr = StaticTabSceneLayerJni.get().init(StaticTabSceneLayer.this);
        }
        assert mNativePtr != 0;
    }

    @Override
    public void destroy() {
        super.destroy();
        mNativePtr = 0;
    }

    @NativeMethods
    interface Natives {
        long init(StaticTabSceneLayer caller);
        void updateTabLayer(long nativeStaticTabSceneLayer, StaticTabSceneLayer caller,
                TabContentManager tabContentManager, int id, boolean canUseLiveLayer,
                int backgroundColor, float x, float y, float staticToViewBlend, float saturation,
                float brightness);
    }
}
