// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.BOTTOM_BAR_HEIGHT;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.IS_SHOWING_OVERVIEW;
import static org.chromium.chrome.features.start_surface.StartSurfaceProperties.TOP_MARGIN;

import android.animation.ObjectAnimator;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.MarginLayoutParams;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** The binder controls the display of the {@link TasksView} in its parent. */
class TasksSurfaceViewBinder {
    private static final long FADE_IN_DURATION_MS = 50;

    /**
     * The view holder holds the parent view and the tasks surface view.
     */
    public static class ViewHolder {
        public final ViewGroup parentView;
        public final View tasksSurfaceView;
        public final View topToolbarPlaceholderView;

        ViewHolder(ViewGroup parentView, View tasksSurfaceView, View topToolbarPlaceholderView) {
            this.parentView = parentView;
            this.tasksSurfaceView = tasksSurfaceView;
            this.topToolbarPlaceholderView = topToolbarPlaceholderView;
        }
    }

    public static void bind(PropertyModel model, ViewHolder viewHolder, PropertyKey propertyKey) {
        if (IS_SHOWING_OVERVIEW == propertyKey) {
            updateLayoutAndVisibility(viewHolder, model);
        } else if (BOTTOM_BAR_HEIGHT == propertyKey) {
            setBottomBarHeight(viewHolder, model.get(BOTTOM_BAR_HEIGHT));
        } else if (TOP_MARGIN == propertyKey) {
            setTopBarHeight(viewHolder, model.get(TOP_MARGIN));
        }
    }

    private static void updateLayoutAndVisibility(ViewHolder viewHolder, PropertyModel model) {
        boolean isShowing = model.get(IS_SHOWING_OVERVIEW);
        if (isShowing && viewHolder.tasksSurfaceView.getParent() == null) {
            viewHolder.parentView.addView(viewHolder.tasksSurfaceView);
            MarginLayoutParams layoutParams =
                    (MarginLayoutParams) viewHolder.tasksSurfaceView.getLayoutParams();
            layoutParams.bottomMargin = model.get(BOTTOM_BAR_HEIGHT);
            setTopBarHeight(viewHolder, model.get(TOP_MARGIN));
        }

        View taskSurfaceView = viewHolder.tasksSurfaceView;
        if (!isShowing) {
            taskSurfaceView.setVisibility(View.GONE);
        } else {
            // TODO(yuezhanggg): Figure out why there is a blink in the tab switcher part when
            // showing overview mode. (crbug.com/995423)
            taskSurfaceView.setAlpha(0f);
            taskSurfaceView.setVisibility(View.VISIBLE);
            final ObjectAnimator taskSurfaceFadeInAnimator =
                    ObjectAnimator.ofFloat(taskSurfaceView, View.ALPHA, 0f, 1f);
            taskSurfaceFadeInAnimator.setDuration(FADE_IN_DURATION_MS);
            taskSurfaceFadeInAnimator.start();
        }
    }

    private static void setBottomBarHeight(ViewHolder viewHolder, int height) {
        MarginLayoutParams layoutParams =
                (MarginLayoutParams) viewHolder.tasksSurfaceView.getLayoutParams();
        if (layoutParams != null) layoutParams.bottomMargin = height;
    }

    private static void setTopBarHeight(ViewHolder viewHolder, int height) {
        ViewGroup.LayoutParams lp = viewHolder.topToolbarPlaceholderView.getLayoutParams();
        if (lp == null) return;

        lp.height = height;
        viewHolder.topToolbarPlaceholderView.setLayoutParams(lp);
    }
}
