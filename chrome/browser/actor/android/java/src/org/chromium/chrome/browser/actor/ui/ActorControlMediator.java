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

    void setContent(String title, String desc) {
        mModel.set(ActorControlProperties.TASK_TITLE, title);
        mModel.set(ActorControlProperties.TASK_STEP_DESCRIPTION, desc);
    }

    void updateStatusIcon(boolean isPaused) {
        int iconRes =
                isPaused ? R.drawable.ic_play_arrow_white_24dp : R.drawable.ic_pause_white_24dp;
        mModel.set(ActorControlProperties.STATUS_ICON_RESOURCE, iconRes);
    }
}
