// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor.ui;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyModel;

/** Mediator for actor control view. */
@NullMarked
public class ActorControlMediator {
    private final PropertyModel mModel;

    ActorControlMediator(PropertyModel model) {
        mModel = model;
    }

    /**
     * Sets the content and state of the actor control view.
     *
     * @param title The title of the actor control view.
     * @param state The PeekViewUiState containing the desired UI properties.
     */
    void setContent(String title, PeekViewUiState state) {
        mModel.set(ActorControlProperties.TASK_TITLE, title);
        mModel.set(ActorControlProperties.PEEK_VIEW_UI_STATE, state);
    }
}
