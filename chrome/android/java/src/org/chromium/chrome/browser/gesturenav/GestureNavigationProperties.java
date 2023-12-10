// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.gesturenav;

import android.gesture.GesturePoint;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableFloatPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Properties used for gesture navigation view model. */
class GestureNavigationProperties {
    /** Gesture navigation action as defined in {@link NavigationHandler.GestureAction}. */
    static final WritableIntPropertyKey ACTION = new WritableIntPropertyKey();

    /**
     * Gesture navigation direction. {@code true} for forward navigation, {@code false} for back.
     */
    static final WritableBooleanPropertyKey DIRECTION = new WritableBooleanPropertyKey();

    /**
     * Whether to allow a sufficiently large pull to trigger the navigation action and
     * animation sequence. Set for {@link GestureAction.RELEASE}.
     */
    static final WritableBooleanPropertyKey ALLOW_NAV = new WritableBooleanPropertyKey();

    /** Amount of total swipe gesture offset. */
    static final WritableFloatPropertyKey BUBBLE_OFFSET = new WritableFloatPropertyKey();

    static final WritableFloatPropertyKey GLOW_OFFSET = new WritableFloatPropertyKey();

    /**
     * Type of arrow bubble according to the action it will take when navigating.
     * @see {@link NavigationBubble#CloseTarget}
     */
    static final WritableIntPropertyKey CLOSE_INDICATOR = new WritableIntPropertyKey();

    /** Current position of gesture action. Used for {@link GestureState.GLOW}. */
    static final WritableObjectPropertyKey<GesturePoint> GESTURE_POS =
            new WritableObjectPropertyKey<>();

    static final PropertyKey[] ALL_KEYS = {
        ACTION, DIRECTION, ALLOW_NAV, BUBBLE_OFFSET, GLOW_OFFSET, CLOSE_INDICATOR, GESTURE_POS
    };
}
