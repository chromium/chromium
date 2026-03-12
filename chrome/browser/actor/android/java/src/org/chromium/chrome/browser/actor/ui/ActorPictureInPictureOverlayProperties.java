// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor.ui;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** {@link PropertyKey} list for ActorPictureInPictureOverlayView. */
@NullMarked
class ActorPictureInPictureOverlayProperties {
    /** The main title text for the current task. */
    public static final PropertyModel.WritableObjectPropertyKey<String> TITLE =
            new PropertyModel.WritableObjectPropertyKey<>();

    /** The task status for the current task. */
    public static final PropertyModel.WritableObjectPropertyKey<String> STATUS_TEXT =
            new PropertyModel.WritableObjectPropertyKey<>();

    /** The visibility of the ActorPicturInPictureOverlay. */
    public static final PropertyModel.WritableBooleanPropertyKey VISIBLE =
            new PropertyModel.WritableBooleanPropertyKey();

    public static final PropertyKey[] ALL_KEYS = {TITLE, STATUS_TEXT, VISIBLE};
}
