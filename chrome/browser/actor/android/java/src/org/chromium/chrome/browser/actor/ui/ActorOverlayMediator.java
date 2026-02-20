// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor.ui;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyModel;

/** Mediator for the Actor Overlay. */
@NullMarked
class ActorOverlayMediator {
    private final PropertyModel mModel;

    /**
     * @param model The PropertyModel to modify.
     */
    public ActorOverlayMediator(PropertyModel model) {
        mModel = model;
    }

    /**
     * Sets the visibility of the overlay.
     *
     * @param visible True to make the overlay visible, false to hide it.
     */
    public void setOverlayVisible(boolean visible) {
        mModel.set(ActorOverlayProperties.VISIBLE, visible);
    }
}
