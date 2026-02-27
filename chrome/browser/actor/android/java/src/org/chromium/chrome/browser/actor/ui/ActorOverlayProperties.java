// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor.ui;

import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;

/** Properties defined for the Actor Overlay model. */
@NullMarked
class ActorOverlayProperties {
    /** Whether the overlay is currently visible. */
    public static final WritableBooleanPropertyKey VISIBLE = new WritableBooleanPropertyKey();

    /** Whether the overlay is allowed to show (e.g., not hidden by a tab switch). */
    public static final WritableBooleanPropertyKey CAN_SHOW = new WritableBooleanPropertyKey();

    /** The top margin of the overlay. */
    public static final WritableIntPropertyKey TOP_MARGIN = new WritableIntPropertyKey();

    /** The bottom margin of the overlay. */
    public static final WritableIntPropertyKey BOTTOM_MARGIN = new WritableIntPropertyKey();

    /** The click listener for the overlay. */
    public static final ReadableObjectPropertyKey<View.OnClickListener> ON_CLICK_LISTENER =
            new ReadableObjectPropertyKey<>();

    /** All keys for the property model. */
    public static final PropertyKey[] ALL_KEYS = {
        VISIBLE, CAN_SHOW, TOP_MARGIN, BOTTOM_MARGIN, ON_CLICK_LISTENER
    };
}
