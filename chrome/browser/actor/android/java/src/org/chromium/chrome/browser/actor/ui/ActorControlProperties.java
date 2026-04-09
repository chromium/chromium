// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor.ui;

import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** {@link PropertyKey} list for ActorControlView. */
@NullMarked
class ActorControlProperties {
    /** The main title text representing the current task. */
    public static final PropertyModel.WritableObjectPropertyKey<String> TASK_TITLE =
            new PropertyModel.WritableObjectPropertyKey<>();

    /** The current state of the Peek View UI, containing description, icon, and button states. */
    public static final PropertyModel.WritableObjectPropertyKey<PeekViewUiState>
            PEEK_VIEW_UI_STATE = new PropertyModel.WritableObjectPropertyKey<>();

    /**
     * The click listener for the play/pause button. The button's appearance and visibility are
     * dictated by the current PEEK_VIEW_UI_STATE.
     */
    public static final PropertyModel.ReadableObjectPropertyKey<View.OnClickListener>
            ON_PLAY_PAUSE_CLICKED = new PropertyModel.ReadableObjectPropertyKey<>();

    /** The click listener for the close button, typically used to dismiss the control. */
    public static final PropertyModel.ReadableObjectPropertyKey<View.OnClickListener>
            ON_CLOSE_CLICKED = new PropertyModel.ReadableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                TASK_TITLE, PEEK_VIEW_UI_STATE, ON_PLAY_PAUSE_CLICKED, ON_CLOSE_CLICKED
            };
}
