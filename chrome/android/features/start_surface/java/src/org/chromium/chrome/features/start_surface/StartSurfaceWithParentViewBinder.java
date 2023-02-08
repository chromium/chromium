// Copyright 2019 The Chromium Authors
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
class StartSurfaceWithParentViewBinder {
    private static final long FADE_IN_DURATION_MS = 50;

    /**
     * The view holder holds the parent view and the tasks surface view.
     */
    public static class ViewHolder {
        public final ViewGroup parentView;
        public final View tasksSurfaceView;
        public final ViewGroup feedSwipeRefreshLayout;

        ViewHolder(ViewGroup parentView, View tasksSurfaceView, ViewGroup feedSwipeRefreshLayout) {
            this.parentView = parentView;
            this.tasksSurfaceView = tasksSurfaceView;
            this.feedSwipeRefreshLayout = feedSwipeRefreshLayout;
        }
    }

    public static void bind(PropertyModel model, ViewHolder viewHolder, PropertyKey propertyKey) {
        if (IS_SHOWING_OVERVIEW == propertyKey) {
            updateLayoutAndVisibility(viewHolder, model);
        } else if (BOTTOM_BAR_HEIGHT == propertyKey) {
            setBottomBarHeight(viewHolder, model.get(BOTTOM_BAR_HEIGHT));
        } else if (TOP_MARGIN == propertyKey) {
            setTopMargin(viewHolder, model.get(TOP_MARGIN));
        }
    }

    private static void updateLayoutAndVisibility(ViewHolder viewHolder, PropertyModel model) {
        boolean isShowing = model.get(IS_SHOWING_OVERVIEW);
        if (isShowing && viewHolder.tasksSurfaceView.getParent() == null) {
            // Insert right above compositor view if present.
            // TODO(crbug.com/1216949): Look into enforcing the z-order of the views.
            int pos = viewHolder.parentView.getChildCount() > 0 ? 1 : 0;
            viewHolder.parentView.addView(viewHolder.tasksSurfaceView, pos);
            MarginLayoutParams layoutParams =
                    (MarginLayoutParams) viewHolder.tasksSurfaceView.getLayoutParams();
            if (layoutParams != null) layoutParams.bottomMargin = model.get(BOTTOM_BAR_HEIGHT);
            setTopMargin(viewHolder, model.get(TOP_MARGIN));
        }

        View taskSurfaceView = viewHolder.tasksSurfaceView;
        View feedSwipeRefreshLayout = viewHolder.feedSwipeRefreshLayout;
        if (!isShowing) {
            taskSurfaceView.setVisibility(View.GONE);
            if (feedSwipeRefreshLayout != null) {
                feedSwipeRefreshLayout.setVisibility(View.GONE);
            }
        } else {
            // We have to set the FeedSwipeRefreshLayout visible even when the Tab switcher is
            // showing. This is because both the primary_tasks_surface_view and
            // secondary_tasks_surface_view are created and the FeedSwipeRefreshLayout is the root
            // view of both surface views.
            if (feedSwipeRefreshLayout != null
                    && feedSwipeRefreshLayout.getVisibility() == View.GONE) {
                feedSwipeRefreshLayout.setVisibility(View.VISIBLE);
            }
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
        if (layoutParams != null) {
            layoutParams.bottomMargin = height;
            viewHolder.tasksSurfaceView.setLayoutParams(layoutParams);
        }
    }

    private static void setTopMargin(ViewHolder viewHolder, int topMargin) {
        MarginLayoutParams layoutParams =
                (MarginLayoutParams) viewHolder.tasksSurfaceView.getLayoutParams();
        if (layoutParams == null) return;

        layoutParams.topMargin = topMargin;
        viewHolder.tasksSurfaceView.setLayoutParams(layoutParams);
    }
}
