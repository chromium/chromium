// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor.ui;

import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Binds model properties to the "Take over task" button. */
@NullMarked
class ActorHandoffButtonViewBinder {
    /**
     * Binds a specific property to the view.
     *
     * @param model The property model.
     * @param button The button to bind to.
     * @param key The property key that changed.
     */
    public static void bind(PropertyModel model, View button, PropertyKey key) {
        if (key == ActorOverlayProperties.TAKE_OVER_TASK_BUTTON_VISIBLE
                || key == ActorOverlayProperties.VISIBLE) {
            // The button only shows with the overlay.
            boolean visible =
                    model.get(ActorOverlayProperties.VISIBLE)
                            && model.get(ActorOverlayProperties.TAKE_OVER_TASK_BUTTON_VISIBLE);
            int visibility = visible ? View.VISIBLE : View.GONE;
            if (button.getVisibility() != visibility) {
                button.setVisibility(visibility);
            }
        } else if (key == ActorOverlayProperties.ON_TAKE_OVER_CLICK_LISTENER) {
            button.setOnClickListener(
                    model.get(ActorOverlayProperties.ON_TAKE_OVER_CLICK_LISTENER));
        }
    }
}
