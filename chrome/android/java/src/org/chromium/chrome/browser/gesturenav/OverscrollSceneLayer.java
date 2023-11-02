// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.gesturenav;

import android.view.View;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.layouts.scene_layer.SceneLayer;
import org.chromium.chrome.browser.layouts.scene_layer.SceneOverlayLayer;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.resources.ResourceManager;

/**
 * {@link SceneOverlayLayer} implementation for gesture navigation overscroll effect.
 */
@JNINamespace("android")
class OverscrollSceneLayer extends SceneOverlayLayer {
    private final View mParentView;

    // NOTE: If you use SceneLayer's native pointer here, the JNI generator will try to
    // downcast using reinterpret_cast<>. We keep a separate pointer to avoid it.
    private long mNativePtr;

    private float mAccumulatedScroll;

    OverscrollSceneLayer(WindowAndroid window, View parentView) {
        mParentView = parentView;
        mNativePtr = OverscrollSceneLayerJni.get().init(OverscrollSceneLayer.this, window);
        assert mNativePtr != 0;
    }

    /**
     * Initialize a new overscroll effect.
     */
    void prepare(float startX, float startY) {
        mAccumulatedScroll = 0.f;
        OverscrollSceneLayerJni.get().prepare(mNativePtr, OverscrollSceneLayer.this, startX, startY,
                mParentView.getWidth(), mParentView.getHeight());
    }

    /**
     * Send down the swipe offset to update animation for overscroll effect.
     * @param resourceManager An object for accessing static and dynamic resources.
     * @param offset Swipe offset from touch events.
     * @return {@code true} if the animation is still in progress; {@code false} if the animation
     *         is completed.
     */
    boolean update(ResourceManager resourceManager, float offset) {
        float xDelta = -(offset - mAccumulatedScroll);
        mAccumulatedScroll = offset;

        // Do not go down with zero delta since it can make the animation jerky. But return true
        // to keep the animation going on.
        if (xDelta == 0.f) return true;
        return OverscrollSceneLayerJni.get().update(
                mNativePtr, OverscrollSceneLayer.this, resourceManager, mAccumulatedScroll, xDelta);
    }

    /** Release the glow effect to recede slowly. */
    void release() {
        OverscrollSceneLayerJni.get().update(mNativePtr, OverscrollSceneLayer.this, null, 0.f, 0.f);
        mAccumulatedScroll = 0.f;
    }

    /** Reset the glow effect. */
    void reset() {
        OverscrollSceneLayerJni.get().onReset(mNativePtr, OverscrollSceneLayer.this);
    }

    @Override
    public void setContentTree(SceneLayer contentTree) {
        OverscrollSceneLayerJni.get().setContentTree(
                mNativePtr, OverscrollSceneLayer.this, contentTree);
    }

    @Override
    protected void initializeNative() {
        // Native side is initialized in the constructor to use the passed WindowAndroid.
    }

    @Override
    public void destroy() {
        super.destroy();
        mNativePtr = 0;
    }

    @NativeMethods
    interface Natives {
        long init(OverscrollSceneLayer caller, WindowAndroid window);
        void prepare(long nativeOverscrollSceneLayer, OverscrollSceneLayer caller, float startX,
                float startY, int width, int height);
        void setContentTree(long nativeOverscrollSceneLayer, OverscrollSceneLayer caller,
                SceneLayer contentTree);
        boolean update(long nativeOverscrollSceneLayer, OverscrollSceneLayer caller,
                ResourceManager resourceManager, float accumulatedScroll, float delta);
        void onReset(long nativeOverscrollSceneLayer, OverscrollSceneLayer caller);
    }
}
