// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor.ui;

import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Binds model properties to the Actor Overlay container and view. */
@NullMarked
class ActorOverlayViewBinder {
    /** View holder containing references to the overlay container and child overlay view. */
    static class ViewHolder {
        public final FrameLayout container;
        public final ActorOverlayView overlayView;

        public ViewHolder(FrameLayout container, ActorOverlayView overlayView) {
            this.container = container;
            this.overlayView = overlayView;
        }
    }

    /**
     * Binds a specific property to the view.
     *
     * @param model The property model.
     * @param viewHolder The view holder to bind to.
     * @param key The property key that changed.
     */
    public static void bind(PropertyModel model, ViewHolder viewHolder, PropertyKey key) {
        if (key == ActorOverlayProperties.VISIBLE) {
            boolean visible = model.get(ActorOverlayProperties.VISIBLE);
            int visibility = visible ? View.VISIBLE : View.GONE;
            if (viewHolder.container.getVisibility() != visibility) {
                viewHolder.container.setVisibility(visibility);
            }
            if (viewHolder.overlayView.getVisibility() != visibility) {
                viewHolder.overlayView.setVisibility(visibility);
            }
        } else if (key == ActorOverlayProperties.LEFT_MARGIN
                || key == ActorOverlayProperties.TOP_MARGIN
                || key == ActorOverlayProperties.RIGHT_MARGIN
                || key == ActorOverlayProperties.BOTTOM_MARGIN) {
            int left = model.get(ActorOverlayProperties.LEFT_MARGIN);
            int top = model.get(ActorOverlayProperties.TOP_MARGIN);
            int right = model.get(ActorOverlayProperties.RIGHT_MARGIN);
            int bottom = model.get(ActorOverlayProperties.BOTTOM_MARGIN);
            // Container takes side UI horizontal offsets and bottom controls margin.
            setMargins(viewHolder.container, left, 0, right, bottom);
            // Overlay view takes top margin to begin below the top controls.
            viewHolder.overlayView.setMargins(0, top, 0, 0);
        } else if (key == ActorOverlayProperties.ON_CLICK_LISTENER) {
            viewHolder.overlayView.setOnClickListener(
                    model.get(ActorOverlayProperties.ON_CLICK_LISTENER));
        }
    }

    private static void setMargins(View view, int left, int top, int right, int bottom) {
        ViewGroup.LayoutParams layoutParams = view.getLayoutParams();
        if (!(layoutParams instanceof ViewGroup.MarginLayoutParams)) {
            return;
        }
        ViewGroup.MarginLayoutParams params = (ViewGroup.MarginLayoutParams) layoutParams;

        if (params.leftMargin != left
                || params.topMargin != top
                || params.rightMargin != right
                || params.bottomMargin != bottom) {
            params.leftMargin = left;
            params.topMargin = top;
            params.rightMargin = right;
            params.bottomMargin = bottom;
            view.setLayoutParams(params);
        }
    }
}
