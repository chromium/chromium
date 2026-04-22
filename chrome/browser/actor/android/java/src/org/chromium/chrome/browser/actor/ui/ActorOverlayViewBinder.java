// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor.ui;

import android.view.View;
import android.view.ViewGroup.MarginLayoutParams;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Binds model properties to the ActorOverlayView. */
@NullMarked
class ActorOverlayViewBinder {
    /**
     * Binds a specific property to the view.
     *
     * @param model The property model.
     * @param view The view to bind to.
     * @param key The property key that changed.
     */
    public static void bind(PropertyModel model, ActorOverlayView view, PropertyKey key) {
        if (key == ActorOverlayProperties.VISIBLE) {
            boolean visible = model.get(ActorOverlayProperties.VISIBLE);
            int visibility = visible ? View.VISIBLE : View.GONE;
            if (view.getVisibility() != visibility) {
                view.setVisibility(visibility);
            }
        } else if (key == ActorOverlayProperties.TOP_MARGIN
                || key == ActorOverlayProperties.BOTTOM_MARGIN) {
            int top = model.get(ActorOverlayProperties.TOP_MARGIN);
            int bottom = model.get(ActorOverlayProperties.BOTTOM_MARGIN);
            MarginLayoutParams params = (MarginLayoutParams) view.getLayoutParams();
            if (params == null || params.topMargin != top || params.bottomMargin != bottom) {
                view.setMargins(top, bottom);
            }
        } else if (key == ActorOverlayProperties.ON_CLICK_LISTENER) {
            view.setOnClickListener(model.get(ActorOverlayProperties.ON_CLICK_LISTENER));
        }
    }
}
