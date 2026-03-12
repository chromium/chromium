// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor.ui;

import static org.chromium.chrome.browser.actor.ui.ActorPictureInPictureOverlayProperties.STATUS_TEXT;
import static org.chromium.chrome.browser.actor.ui.ActorPictureInPictureOverlayProperties.TITLE;
import static org.chromium.chrome.browser.actor.ui.ActorPictureInPictureOverlayProperties.VISIBLE;

import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Binds model properties to the ActorPictureInPictureOverlayView. */
@NullMarked
class ActorPictureInPictureOverlayViewBinder {
    /**
     * Binds a specific property to the view.
     *
     * @param model The property model.
     * @param view The view to bind to.
     * @param key The property key that changed.
     */
    public static void bind(
            PropertyModel model, ActorPictureInPictureOverlayView view, PropertyKey propertyKey) {
        if (TITLE == propertyKey) {
            view.setTitle(model.get(TITLE));
        } else if (STATUS_TEXT == propertyKey) {
            view.setStatus(model.get(STATUS_TEXT));
        } else if (propertyKey == VISIBLE) {
            view.setVisibility(model.get(VISIBLE) ? View.VISIBLE : View.GONE);
        }
    }
}
