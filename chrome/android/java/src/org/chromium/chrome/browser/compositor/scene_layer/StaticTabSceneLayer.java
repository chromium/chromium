// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.scene_layer;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.compositor.layouts.components.LayoutTab;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.layouts.scene_layer.SceneLayer;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * A SceneLayer to render a static tab.
 */
@JNINamespace("android")
public class StaticTabSceneLayer extends SceneLayer {
    /**
     * ViewBinder for the StaticTabSceneLayer.
     * @param model The model to bind.
     * @param view The View that the model bind to.
     * @param propertyKey The property of the view that changed. This is NULL until SceneLayer is
     *                    able to do partial update.
     */
    public static void bind(
            PropertyModel model, StaticTabSceneLayer view, PropertyKey propertyKey) {
        view.update(model);
    }

    // NOTE: If you use SceneLayer's native pointer here, the JNI generator will try to
    // downcast using reinterpret_cast<>. We keep a separate pointer to avoid it.
    private long mNativePtr;

    /**
     * Update {@link StaticTabSceneLayer} with the given {@link PropertyModel}.
     * @param model         The {@link PropertyModel} to use.
     */
    public void update(PropertyModel model) {
        if (model == null) {
            return;
        }

        float x = model.get(LayoutTab.RENDER_X) * LayoutTab.sDpToPx;
        float y = model.get(LayoutTab.CONTENT_OFFSET)
                + model.get(LayoutTab.RENDER_Y) * LayoutTab.sDpToPx;

        StaticTabSceneLayerJni.get().updateTabLayer(mNativePtr, StaticTabSceneLayer.this,
                model.get(LayoutTab.TAB_ID), model.get(LayoutTab.CAN_USE_LIVE_TEXTURE),
                model.get(LayoutTab.BACKGROUND_COLOR), x, y,
                model.get(LayoutTab.STATIC_TO_VIEW_BLEND), model.get(LayoutTab.SATURATION),
                model.get(LayoutTab.BRIGHTNESS));
    }

    /**
     * Set {@link TabContentManager}.
     * @param tabContentManager {@link TabContentManager} to set.
     */
    public void setTabContentManager(TabContentManager tabContentManager) {
        StaticTabSceneLayerJni.get().setTabContentManager(
                mNativePtr, StaticTabSceneLayer.this, tabContentManager);
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
        void updateTabLayer(long nativeStaticTabSceneLayer, StaticTabSceneLayer caller, int id,
                boolean canUseLiveLayer, int backgroundColor, float x, float y,
                float staticToViewBlend, float saturation, float brightness);
        void setTabContentManager(long nativeStaticTabSceneLayer, StaticTabSceneLayer caller,
                TabContentManager tabContentManager);
    }
}
