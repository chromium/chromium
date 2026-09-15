// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor.ui;

import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Binds model properties to the ActorHandoffButtonView. */
@NullMarked
class ActorHandoffButtonViewBinder {
    /**
     * Binds a specific property to the view.
     *
     * @param model The property model.
     * @param view The view to bind to.
     * @param key The property key that changed.
     */
    public static void bind(PropertyModel model, ActorHandoffButtonView view, PropertyKey key) {
        if (key == ActorOverlayProperties.TAKE_OVER_TASK_BUTTON_VISIBLE) {
            boolean visible = model.get(ActorOverlayProperties.TAKE_OVER_TASK_BUTTON_VISIBLE);
            int visibility = visible ? View.VISIBLE : View.GONE;
            if (view.getVisibility() != visibility) {
                view.setVisibility(visibility);
            }
        } else if (key == ActorOverlayProperties.ON_TAKE_OVER_CLICK_LISTENER) {
            View button = view.getButton();
            if (button != null) {
                button.setOnClickListener(
                        model.get(ActorOverlayProperties.ON_TAKE_OVER_CLICK_LISTENER));
            }
        }
    }
}
