// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor.ui;

import android.content.Context;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** ViewBinder for ActorControlView. */
@NullMarked
public class ActorControlViewBinder {

    /**
     * This method binds the given model to the given view.
     *
     * @param model The model to use.
     * @param view The View to use.
     * @param propertyKey The key for the property to update for.
     */
    public static void bind(PropertyModel model, ActorControlView view, PropertyKey propertyKey) {
        if (ActorControlProperties.TASK_TITLE == propertyKey) {
            view.setTitle(model.get(ActorControlProperties.TASK_TITLE));
        } else if (ActorControlProperties.PEEK_VIEW_UI_STATE == propertyKey) {
            PeekViewUiState state = model.get(ActorControlProperties.PEEK_VIEW_UI_STATE);
            Context context = view.getContext();
            view.setStepDescription(state.getDescription(context));
            view.setStatusIconResource(state.iconResId);
        } else if (ActorControlProperties.ON_PLAY_PAUSE_CLICKED == propertyKey) {
            view.setPlayPauseListener(model.get(ActorControlProperties.ON_PLAY_PAUSE_CLICKED));
        } else if (ActorControlProperties.ON_CLOSE_CLICKED == propertyKey) {
            view.setCloseClickListener(model.get(ActorControlProperties.ON_CLOSE_CLICKED));
        }
    }
}
