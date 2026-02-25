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

    /** A sub-label describing the specific step or status of the current task. */
    public static final PropertyModel.WritableObjectPropertyKey<String> TASK_STEP_DESCRIPTION =
            new PropertyModel.WritableObjectPropertyKey<>();

    /** Whether the task is currently paused. */
    public static final PropertyModel.WritableBooleanPropertyKey IS_TASK_PAUSED =
            new PropertyModel.WritableBooleanPropertyKey();

    /** The click listener for the close button, typically used to dismiss the control. */
    public static final PropertyModel.ReadableObjectPropertyKey<View.OnClickListener>
            ON_PLAY_PAUSE_CLICKED = new PropertyModel.ReadableObjectPropertyKey<>();

    /** The click listener for the close button, typically used to dismiss the control. */
    public static final PropertyModel.ReadableObjectPropertyKey<View.OnClickListener>
            ON_CLOSE_CLICKED = new PropertyModel.ReadableObjectPropertyKey<>();

    /** The resource ID for the status icon (e.g., play or pause). */
    public static final PropertyModel.WritableIntPropertyKey STATUS_ICON_RESOURCE =
            new PropertyModel.WritableIntPropertyKey();

    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                TASK_TITLE,
                TASK_STEP_DESCRIPTION,
                IS_TASK_PAUSED,
                ON_PLAY_PAUSE_CLICKED,
                ON_CLOSE_CLICKED,
                STATUS_ICON_RESOURCE
            };
}
