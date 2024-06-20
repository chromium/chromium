// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud.player.mini;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** UI property keys specific to the mini player. */
class Properties {
    // VisibilityState for signaling the progress of transitions for the player as a whole.
    public static final WritableIntPropertyKey VISIBILITY = new WritableIntPropertyKey();
    public static final WritableBooleanPropertyKey ANIMATE_VISIBILITY_CHANGES =
            new WritableBooleanPropertyKey();
    public static final WritableObjectPropertyKey<MiniPlayerMediator> MEDIATOR =
            new WritableObjectPropertyKey<>();
    // View visibility of the mini player Android view.
    public static final WritableIntPropertyKey ANDROID_VIEW_VISIBILITY =
            new WritableIntPropertyKey();
    // Whether to draw the composited view.
    public static final WritableBooleanPropertyKey COMPOSITED_VIEW_VISIBLE =
            new WritableBooleanPropertyKey();
    public static final WritableBooleanPropertyKey CONTENTS_OPAQUE =
            new WritableBooleanPropertyKey();
    public static final WritableIntPropertyKey BACKGROUND_COLOR_ARGB = new WritableIntPropertyKey();
    public static final WritableIntPropertyKey HEIGHT = new WritableIntPropertyKey();
    // The y-offset that mini player layout need to shift up.
    public static final WritableIntPropertyKey Y_OFFSET = new WritableIntPropertyKey();

    public static final PropertyKey[] ALL_KEYS = {
        VISIBILITY,
        ANIMATE_VISIBILITY_CHANGES,
        MEDIATOR,
        ANDROID_VIEW_VISIBILITY,
        COMPOSITED_VIEW_VISIBLE,
        CONTENTS_OPAQUE,
        BACKGROUND_COLOR_ARGB,
        HEIGHT,
        Y_OFFSET
    };
}
