// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.BOTTOM_BAR_HEIGHT;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.EXPLORE_SURFACE_COORDINATOR;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.IS_BOTTOM_BAR_VISIBLE;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.IS_EXPLORE_SURFACE_VISIBLE;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.IS_SHOWING_OVERVIEW;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.RESET_FEED_SURFACE_SCROLL_POSITION;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.TOP_MARGIN;

import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.recyclerview.widget.RecyclerView;

import org.chromium.ui.UiUtils;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Binder for the explore surface. */
class ExploreSurfaceViewBinder {
    public static void bind(PropertyModel model, ViewGroup parentView, PropertyKey propertyKey) {
        if (propertyKey == IS_EXPLORE_SURFACE_VISIBLE) {
            setVisibility(parentView, model,
                    model.get(IS_EXPLORE_SURFACE_VISIBLE) && model.get(IS_SHOWING_OVERVIEW));
        } else if (propertyKey == IS_SHOWING_OVERVIEW) {
            setVisibility(parentView, model,
                    model.get(IS_EXPLORE_SURFACE_VISIBLE) && model.get(IS_SHOWING_OVERVIEW));
        } else if (propertyKey == TOP_MARGIN) {
            setTopMargin(model);
        } else if (propertyKey == RESET_FEED_SURFACE_SCROLL_POSITION) {
            resetScrollPosition(model);
        }
    }

    /**
     * Set the explore surface visibility.
     * @param parentView The parent view of the feed.
     * @param model The property model.
     * @param isShowing Whether set the surface to visible or not.
     */
    // TODO(crbug.com/982018): Attach feed directly to TasksSurfaceContainerView
    // and get rid of tasks_surface_body to improve performance.
    private static void setVisibility(
            ViewGroup parentView, PropertyModel model, boolean isShowing) {
        if (model.get(EXPLORE_SURFACE_COORDINATOR) == null) return;

        View feedSurfaceView = model.get(EXPLORE_SURFACE_COORDINATOR).getView();
        assert feedSurfaceView != null;
        if (isShowing) {
            parentView.addView(feedSurfaceView);
            // We only need to make room for the top and bottom bar if in the TWOPANE surface {@link
            // SurfaceMode.TWO_PANES}. The bottom bar is only visible in the TWOPANE surface  {@link
            // SurfaceMode.TWO_PANES}.
            if (model.get(IS_BOTTOM_BAR_VISIBLE)) {
                FrameLayout.LayoutParams layoutParams =
                        (FrameLayout.LayoutParams) feedSurfaceView.getLayoutParams();
                layoutParams.bottomMargin = model.get(BOTTOM_BAR_HEIGHT);
                layoutParams.topMargin = model.get(TOP_MARGIN);
            }
        } else {
            UiUtils.removeViewFromParent(feedSurfaceView);
        }
    }

    private static void setTopMargin(PropertyModel model) {
        if (model.get(EXPLORE_SURFACE_COORDINATOR) == null) return;
        if (!model.get(IS_BOTTOM_BAR_VISIBLE)) return;

        View feedSurfaceView = model.get(EXPLORE_SURFACE_COORDINATOR).getView();
        assert feedSurfaceView != null;
        FrameLayout.LayoutParams layoutParams =
                (FrameLayout.LayoutParams) feedSurfaceView.getLayoutParams();
        if (layoutParams == null) return;

        layoutParams.topMargin = model.get(TOP_MARGIN);
        feedSurfaceView.setLayoutParams(layoutParams);
    }

    private static void resetScrollPosition(PropertyModel model) {
        ExploreSurfaceCoordinator exploreSurfaceCoordinator =
                model.get(EXPLORE_SURFACE_COORDINATOR);
        if (exploreSurfaceCoordinator == null) return;

        RecyclerView feedStreamView = exploreSurfaceCoordinator.getRecyclerView();
        if (feedStreamView != null) {
            feedStreamView.scrollToPosition(0);
        }
    }
}
