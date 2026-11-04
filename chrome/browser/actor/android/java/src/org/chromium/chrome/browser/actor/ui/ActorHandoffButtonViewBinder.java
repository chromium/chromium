// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor.ui;

import android.view.View;
import android.view.ViewGroup;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.browser_ui.widget.floating_pill.FloatingPillContainer;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Binds model properties to the handoff button's {@link FloatingPillContainer}. */
@NullMarked
class ActorHandoffButtonViewBinder {
    /**
     * Binds a specific property to the view.
     *
     * @param model The property model.
     * @param view The view to bind to.
     * @param key The property key that changed.
     */
    public static void bind(PropertyModel model, FloatingPillContainer view, PropertyKey key) {
        if (key == ActorOverlayProperties.TAKE_OVER_TASK_BUTTON_VISIBLE) {
            boolean visible = model.get(ActorOverlayProperties.TAKE_OVER_TASK_BUTTON_VISIBLE);
            int visibility = visible ? View.VISIBLE : View.GONE;
            if (view.getVisibility() != visibility) {
                view.setVisibility(visibility);
            }
        } else if (key == ActorOverlayProperties.ON_TAKE_OVER_CLICK_LISTENER) {
            View button = view.findViewById(R.id.take_over_task_button);
            if (button != null) {
                button.setOnClickListener(
                        model.get(ActorOverlayProperties.ON_TAKE_OVER_CLICK_LISTENER));
            }
        } else if (key == ActorOverlayProperties.HANDOFF_BUTTON_TOP_MARGIN) {
            int topMargin = model.get(ActorOverlayProperties.HANDOFF_BUTTON_TOP_MARGIN);
            ViewGroup.LayoutParams layoutParams = view.getLayoutParams();
            if (layoutParams instanceof ViewGroup.MarginLayoutParams) {
                ViewGroup.MarginLayoutParams params = (ViewGroup.MarginLayoutParams) layoutParams;
                if (params.topMargin != topMargin) {
                    params.topMargin = topMargin;
                    view.setLayoutParams(params);
                }
            }
        }
    }
}
