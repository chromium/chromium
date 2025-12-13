// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.bookmarks.bar;

import android.graphics.RectF;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.cc.input.OffsetTag;
import org.chromium.chrome.browser.layouts.SceneOverlay;
import org.chromium.chrome.browser.layouts.scene_layer.SceneLayer;
import org.chromium.chrome.browser.layouts.scene_layer.SceneOverlayLayer;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.resources.ResourceManager;

/**
 * The Java-side representation of the scene layer for drawing snapshots of the Android widgets of
 * the Bookmark Bar using the C++-side renderer. This is used during scrolls for a smooth user
 * experience. Snapshots are provided as uncompressed Bitmaps, and this class minimizes the
 * frequency of updates and the size of the snapshots to minimize performance impact.
 */
@JNINamespace("android")
@NullMarked
public class BookmarkBarSceneLayer extends SceneOverlayLayer implements SceneOverlay {

    private long mNativePtr;
    private final ResourceManager mResourceManager;
    private boolean mIsVisible;

    /** A simple view binder that pushes the whole model to be updated. */
    public static void bind(
            PropertyModel model, BookmarkBarSceneLayer view, @Nullable PropertyKey key) {
        view.updateProperties(model);
    }

    public BookmarkBarSceneLayer(ResourceManager resourceManager) {
        mResourceManager = resourceManager;
    }

    /**
     * Sets the visibility of the scene layer. This is stored locally to prevent frequent updates to
     * the snapshot when they are not necessary, and also sent to the C++-side layer to update the
     * current rendering.
     *
     * @param isVisible The new visibility of the scene layer.
     */
    public void setVisibility(boolean isVisible) {
        if (mNativePtr == 0) return;
        if (mIsVisible == isVisible) return;

        mIsVisible = isVisible;
        if (mIsVisible) {
            BookmarkBarSceneLayerJni.get().showBookmarkBar(mNativePtr);
        } else {
            BookmarkBarSceneLayerJni.get().hideBookmarkBar(mNativePtr);
        }
    }

    /** Push all information about the scene layer / snapshot to native-side code at once. */
    public void updateProperties(PropertyModel model) {
        if (mNativePtr == 0) return;
        if (!mIsVisible) return;

        BookmarkBarSceneLayerJni.get()
                .updateBookmarkBarLayer(
                        mNativePtr,
                        mResourceManager,
                        model.get(BookmarkBarSceneLayerProperties.RESOURCE_ID),
                        model.get(BookmarkBarSceneLayerProperties.BACKGROUND_COLOR),
                        model.get(BookmarkBarSceneLayerProperties.SCENE_LAYER_OFFSET_HEIGHT),
                        model.get(BookmarkBarSceneLayerProperties.SCENE_LAYER_WIDTH),
                        model.get(BookmarkBarSceneLayerProperties.SCENE_LAYER_HEIGHT),
                        model.get(BookmarkBarSceneLayerProperties.SNAPSHOT_OFFSET_WIDTH),
                        model.get(BookmarkBarSceneLayerProperties.SNAPSHOT_OFFSET_HEIGHT),
                        model.get(BookmarkBarSceneLayerProperties.HAIRLINE_HEIGHT),
                        model.get(BookmarkBarSceneLayerProperties.HAIRLINE_BACKGROUND_COLOR),
                        model.get(BookmarkBarSceneLayerProperties.OFFSET_TAG));
    }

    // SceneLayer overrides:

    @Override
    protected void initializeNative() {
        if (mNativePtr == 0) {
            mNativePtr = BookmarkBarSceneLayerJni.get().init(this);
        }
    }

    @Override
    public void destroy() {
        super.destroy();
        mNativePtr = 0;
    }

    // SceneOverlayLayer implementation:

    @Override
    public void setContentTree(SceneLayer contentTree) {
        if (mNativePtr == 0) return;
        BookmarkBarSceneLayerJni.get().setContentTree(mNativePtr, contentTree);
    }

    // SceneOverlay implementation:

    @Override
    public @Nullable SceneOverlayLayer getUpdatedSceneOverlayTree(
            RectF viewport, RectF visibleViewport, ResourceManager resourceManager) {
        return this;
    }

    @Override
    public boolean isSceneOverlayTreeShowing() {
        return mIsVisible;
    }

    @Override
    public void onSizeChanged(
            float width, float height, float visibleViewportOffsetY, int orientation) {}

    @NativeMethods
    public interface Natives {
        long init(BookmarkBarSceneLayer self);

        void setContentTree(long nativeBookmarkBarSceneLayer, SceneLayer contentTree);

        void updateBookmarkBarLayer(
                long nativeBookmarkBarSceneLayer,
                ResourceManager manager,
                int viewResourceId,
                int sceneLayerBackgroundColor,
                int sceneLayerOffsetHeight,
                int sceneLayerWidth,
                int sceneLayerHeight,
                int snapshotOffsetWidth,
                int snapshotOffsetHeight,
                int hairlineHeight,
                int hairlineBackgroundColor,
                @Nullable OffsetTag offsetTag);

        void hideBookmarkBar(long nativeBookmarkBarSceneLayer);

        void showBookmarkBar(long nativeBookmarkBarSceneLayer);
    }
}
