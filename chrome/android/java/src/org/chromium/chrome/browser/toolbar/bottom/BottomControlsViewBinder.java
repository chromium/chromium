// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.bottom;

import android.view.View;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.scene_layer.ScrollingBottomViewSceneLayer;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

class BottomControlsViewBinder {
    /**
     * A wrapper class that holds a {@link ScrollingBottomViewResourceFrameLayout}
     * and a composited layer to be used with the {@link BottomControlsViewBinder}.
     */
    static class ViewHolder {
        /** A handle to the Android View based version of the bottom controls. */
        public final ScrollingBottomViewResourceFrameLayout root;

        /** A handle to the composited bottom controls layer. */
        public ScrollingBottomViewSceneLayer sceneLayer;

        /**
         * @param bottomControlsRootView The Android View based bottom controls.
         */
        public ViewHolder(ScrollingBottomViewResourceFrameLayout bottomControlsRootView) {
            root = bottomControlsRootView;
        }
    }

    static void bind(PropertyModel model, ViewHolder view, PropertyKey propertyKey) {
        if (BottomControlsProperties.BOTTOM_CONTROLS_CONTAINER_HEIGHT_PX == propertyKey) {
            View bottomControlsWrapper = view.root.findViewById(R.id.bottom_controls_wrapper);
            bottomControlsWrapper.getLayoutParams().height =
                    model.get(BottomControlsProperties.BOTTOM_CONTROLS_CONTAINER_HEIGHT_PX);
        } else if (BottomControlsProperties.BOTTOM_CONTROLS_HEIGHT_PX == propertyKey) {
            View bottomContainerSlot = view.root.findViewById(R.id.bottom_toolbar);
            bottomContainerSlot.getLayoutParams().height =
                    model.get(BottomControlsProperties.BOTTOM_CONTROLS_HEIGHT_PX);
        } else if (BottomControlsProperties.Y_OFFSET == propertyKey) {
            // Native may not have completely initialized by the time this is set.
            if (view.sceneLayer == null) return;
            view.sceneLayer.setYOffset(model.get(BottomControlsProperties.Y_OFFSET));
        } else if (BottomControlsProperties.ANDROID_VIEW_VISIBLE == propertyKey) {
            view.root.setVisibility(model.get(BottomControlsProperties.ANDROID_VIEW_VISIBLE)
                            ? View.VISIBLE
                            : View.INVISIBLE);
        } else if (BottomControlsProperties.COMPOSITED_VIEW_VISIBLE == propertyKey) {
            if (view.sceneLayer == null) return;
            final boolean showCompositedView =
                    model.get(BottomControlsProperties.COMPOSITED_VIEW_VISIBLE);
            view.sceneLayer.setIsVisible(showCompositedView);
            if (model.get(BottomControlsProperties.TOOLBAR_SWIPE_LAYOUT) != null) {
                model.get(BottomControlsProperties.TOOLBAR_SWIPE_LAYOUT)
                        .setBottomToolbarSceneLayersVisibility(showCompositedView);
            }
            model.get(BottomControlsProperties.LAYOUT_MANAGER).requestUpdate();
        } else if (BottomControlsProperties.LAYOUT_MANAGER == propertyKey) {
            assert view.sceneLayer == null;
            view.sceneLayer =
                    new ScrollingBottomViewSceneLayer(view.root, view.root.getTopShadowHeight());
            view.sceneLayer.setIsVisible(
                    model.get(BottomControlsProperties.COMPOSITED_VIEW_VISIBLE));
            model.get(BottomControlsProperties.LAYOUT_MANAGER)
                    .addSceneOverlayToBack(view.sceneLayer);
        } else if (BottomControlsProperties.TOOLBAR_SWIPE_LAYOUT == propertyKey) {
            assert view.sceneLayer != null;
            assert model.get(BottomControlsProperties.TOOLBAR_SWIPE_LAYOUT) != null;
            model.get(BottomControlsProperties.TOOLBAR_SWIPE_LAYOUT)
                    .setBottomToolbarSceneLayers(new ScrollingBottomViewSceneLayer(view.sceneLayer),
                            new ScrollingBottomViewSceneLayer(view.sceneLayer),
                            model.get(BottomControlsProperties.COMPOSITED_VIEW_VISIBLE));
        } else if (BottomControlsProperties.RESOURCE_MANAGER == propertyKey) {
            model.get(BottomControlsProperties.RESOURCE_MANAGER)
                    .getDynamicResourceLoader()
                    .registerResource(view.root.getId(), view.root.getResourceAdapter());
        } else if (BottomControlsProperties.TOOLBAR_SWIPE_HANDLER == propertyKey) {
            view.root.setSwipeDetector(model.get(BottomControlsProperties.TOOLBAR_SWIPE_HANDLER));
        } else {
            assert false : "Unhandled property detected in BottomControlsViewBinder!";
        }
    }
}
