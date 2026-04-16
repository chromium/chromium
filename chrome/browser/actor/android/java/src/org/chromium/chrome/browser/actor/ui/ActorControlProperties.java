// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor.ui;


import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** {@link PropertyKey} list for ActorControlView. */
@NullMarked
class ActorControlProperties {
    /** The main title text representing the current task. */
    public static final WritableObjectPropertyKey<String> TASK_TITLE =
            new WritableObjectPropertyKey<>();

    /** The current state of the Peek View UI, containing description, icon, and button states. */
    public static final WritableObjectPropertyKey<PeekViewUiState> PEEK_VIEW_UI_STATE =
            new WritableObjectPropertyKey<>();

    /**
     * The click listener for the play/pause button. The button's appearance and visibility are
     * dictated by the current PEEK_VIEW_UI_STATE.
     */
    public static final ReadableObjectPropertyKey<Runnable> ON_ACTOR_CONTROL_CLICKED =
            new ReadableObjectPropertyKey<>();

    /** The click listener for the close button, typically used to dismiss the control. */
    public static final ReadableObjectPropertyKey<Runnable> ON_CLOSE_CLICKED =
            new ReadableObjectPropertyKey<>();

    /** The click listener for the peek view. */
    public static final ReadableObjectPropertyKey<Runnable> ON_PEEK_VIEW_CLICKED =
            new ReadableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                TASK_TITLE,
                PEEK_VIEW_UI_STATE,
                ON_ACTOR_CONTROL_CLICKED,
                ON_CLOSE_CLICKED,
                ON_PEEK_VIEW_CLICKED,
            };
}
