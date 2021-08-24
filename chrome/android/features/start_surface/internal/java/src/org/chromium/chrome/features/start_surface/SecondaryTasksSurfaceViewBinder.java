// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.IS_SECONDARY_SURFACE_VISIBLE;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.IS_SHOWING_OVERVIEW;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.TOP_MARGIN;

import android.view.View;
import android.view.ViewGroup.MarginLayoutParams;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** The binder controls the display of the secondary {@link TasksView} in its parent. */
class SecondaryTasksSurfaceViewBinder {
    public static void bind(PropertyModel model, TasksSurfaceViewBinder.ViewHolder viewHolder,
            PropertyKey propertyKey) {
        if (IS_SECONDARY_SURFACE_VISIBLE == propertyKey) {
            updateVisibility(viewHolder, model);
        } else if (IS_SHOWING_OVERVIEW == propertyKey) {
            updateVisibility(viewHolder, model);
        } else if (TOP_MARGIN == propertyKey) {
            setTopBarHeight(viewHolder, model.get(TOP_MARGIN));
        }
    }

    private static void updateVisibility(
            TasksSurfaceViewBinder.ViewHolder viewHolder, PropertyModel model) {
        boolean isShowing =
                model.get(IS_SHOWING_OVERVIEW) && model.get(IS_SECONDARY_SURFACE_VISIBLE);
        if (isShowing && viewHolder.tasksSurfaceView.getParent() == null) {
            viewHolder.parentView.addView(viewHolder.tasksSurfaceView);
            setTopBarHeight(viewHolder, model.get(TOP_MARGIN));
        }

        // We don't need to handle toolbar scrolling problem on secondary tasks surface so
        // topToolbarPlaceholderView is not needed.
        viewHolder.topToolbarPlaceholderView.setVisibility(View.GONE);
        viewHolder.tasksSurfaceView.setVisibility(isShowing ? View.VISIBLE : View.GONE);
    }

    private static void setTopBarHeight(TasksSurfaceViewBinder.ViewHolder viewHolder, int height) {
        MarginLayoutParams layoutParams =
                (MarginLayoutParams) viewHolder.tasksSurfaceView.getLayoutParams();
        if (layoutParams == null) return;

        layoutParams.topMargin = height;
        viewHolder.tasksSurfaceView.setLayoutParams(layoutParams);
    }
}
