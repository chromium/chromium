// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.BOTTOM_BAR_HEIGHT;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.IS_SHOWING_OVERVIEW;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.TOP_BAR_HEIGHT;

import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.MarginLayoutParams;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** The binder controls the display of the {@link TasksView} in its parent. */
class TasksSurfaceViewBinder {
    /**
     * The view holder holds the parent view and the tasks surface view.
     */
    public static class ViewHolder {
        public final ViewGroup parentView;
        public final View tasksSurfaceView;

        ViewHolder(ViewGroup parentView, View tasksSurfaceView) {
            this.parentView = parentView;
            this.tasksSurfaceView = tasksSurfaceView;
        }
    }

    public static void bind(PropertyModel model, ViewHolder viewHolder, PropertyKey propertyKey) {
        if (IS_SHOWING_OVERVIEW == propertyKey) {
            updateLayoutAndVisibility(viewHolder, model, model.get(IS_SHOWING_OVERVIEW));
        } else if (BOTTOM_BAR_HEIGHT == propertyKey) {
            setBottomBarHeight(viewHolder, model.get(BOTTOM_BAR_HEIGHT));
        }
    }

    private static void updateLayoutAndVisibility(
            ViewHolder viewHolder, PropertyModel model, boolean isShowing) {
        if (isShowing && viewHolder.tasksSurfaceView.getParent() == null) {
            viewHolder.parentView.addView(viewHolder.tasksSurfaceView);
            MarginLayoutParams layoutParams =
                    (MarginLayoutParams) viewHolder.tasksSurfaceView.getLayoutParams();
            layoutParams.bottomMargin = model.get(BOTTOM_BAR_HEIGHT);
            layoutParams.topMargin = model.get(TOP_BAR_HEIGHT);
        }

        viewHolder.tasksSurfaceView.setVisibility(isShowing ? View.VISIBLE : View.GONE);
    }

    private static void setBottomBarHeight(ViewHolder viewHolder, int height) {
        MarginLayoutParams layoutParams =
                (MarginLayoutParams) viewHolder.tasksSurfaceView.getLayoutParams();
        if (layoutParams != null) layoutParams.bottomMargin = height;
    }
}
