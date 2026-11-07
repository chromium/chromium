// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.gesturenav;

import android.view.View;

import org.jni_zero.JNINamespace;
import org.jni_zero.JniPtr;
import org.jni_zero.JniRawPtr;
import org.jni_zero.JniType;
import org.jni_zero.JniTypeToken;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.layouts.scene_layer.SceneLayer;
import org.chromium.chrome.browser.layouts.scene_layer.SceneOverlayLayer;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.resources.ResourceManager;

/** {@link SceneOverlayLayer} implementation for gesture navigation overscroll effect. */
@JNINamespace("android")
@NullMarked
class OverscrollSceneLayer extends SceneOverlayLayer {
    @JniType("::android::OverscrollSceneLayer")
    interface NativeOverscrollSceneLayer extends JniTypeToken {}

    private final View mParentView;

    // Non-owning typed alias; SceneLayer owns and frees the native object.
    private final JniRawPtr<NativeOverscrollSceneLayer> mNativePtr;

    private float mAccumulatedScroll;

    OverscrollSceneLayer(WindowAndroid window, View parentView) {
        mParentView = parentView;
        mNativePtr = OverscrollSceneLayerJni.get().init(this, window);
    }

    /** Initialize a new overscroll effect. */
    void prepare(float startX, float startY) {
        mAccumulatedScroll = 0.f;
        OverscrollSceneLayerJni.get()
                .prepare(
                        mNativePtr,
                        startX,
                        startY,
                        mParentView.getWidth(),
                        mParentView.getHeight());
    }

    /**
     * Send down the swipe offset to update animation for overscroll effect.
     *
     * @param resourceManager An object for accessing static and dynamic resources.
     * @param offset Swipe offset from touch events.
     * @return {@code true} if the animation is still in progress; {@code false} if the animation is
     *     completed.
     */
    boolean update(ResourceManager resourceManager, float offset) {
        float xDelta = -(offset - mAccumulatedScroll);
        mAccumulatedScroll = offset;

        // Do not go down with zero delta since it can make the animation jerky. But return true
        // to keep the animation going on.
        if (xDelta == 0.f) return true;
        return OverscrollSceneLayerJni.get()
                .update(mNativePtr, resourceManager, mAccumulatedScroll, xDelta);
    }

    /** Release the glow effect to recede slowly. */
    void release() {
        OverscrollSceneLayerJni.get().update(mNativePtr, null, 0.f, 0.f);
        mAccumulatedScroll = 0.f;
    }

    /** Reset the glow effect. */
    void reset() {
        OverscrollSceneLayerJni.get().onReset(mNativePtr);
    }

    @Override
    public void setContentTree(SceneLayer contentTree) {
        OverscrollSceneLayerJni.get().setContentTree(mNativePtr, contentTree);
    }

    @Override
    protected void initializeNative() {
        // Native side is initialized in the constructor to use the passed WindowAndroid.
    }

    @Override
    public void destroy() {
        mNativePtr.release();
        super.destroy();
    }

    @NativeMethods
    interface Natives {
        JniRawPtr<NativeOverscrollSceneLayer> init(OverscrollSceneLayer self, WindowAndroid window);

        void prepare(
                JniPtr<NativeOverscrollSceneLayer> self,
                float startX,
                float startY,
                int width,
                int height);

        void setContentTree(JniPtr<NativeOverscrollSceneLayer> self, SceneLayer contentTree);

        boolean update(
                JniPtr<NativeOverscrollSceneLayer> self,
                @Nullable ResourceManager resourceManager,
                float accumulatedScroll,
                float delta);

        void onReset(JniPtr<NativeOverscrollSceneLayer> self);
    }
}
