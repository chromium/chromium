// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor.ui;

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
            view.setVisible(model.get(ActorOverlayProperties.VISIBLE));
        }
    }
}
