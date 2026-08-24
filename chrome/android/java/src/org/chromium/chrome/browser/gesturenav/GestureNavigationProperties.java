// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.gesturenav;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.gesturenav.NavigationHandler.GestureAction;
import org.chromium.ui.OverscrollActivationStatus;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableFloatPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntDefPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;

/** Properties used for gesture navigation view model. */
@NullMarked
class GestureNavigationProperties {
    /** Gesture navigation action as defined in {@link NavigationHandler.GestureAction}. */
    static final WritableIntDefPropertyKey<GestureAction> ACTION =
            new WritableIntDefPropertyKey<>(GestureAction.RESET_BUBBLE);

    /**
     * Gesture navigation direction. {@code true} for forward navigation, {@code false} for back.
     */
    static final WritableBooleanPropertyKey DIRECTION = new WritableBooleanPropertyKey();

    /** Gesture navigation edge as defined in {@link BackGestureEventSwipeEdge}. */
    static final WritableIntPropertyKey EDGE = new WritableIntPropertyKey();

    /** Overscroll activation status as defined in {@link OverscrollActivationStatus}. */
    static final WritableIntDefPropertyKey<OverscrollActivationStatus> ACTIVATION_STATUS =
            new WritableIntDefPropertyKey<>(OverscrollActivationStatus.DISALLOW_ACTIVATION);

    /** Amount of total swipe gesture offset. */
    static final WritableFloatPropertyKey BUBBLE_OFFSET = new WritableFloatPropertyKey();

    /**
     * Type of arrow bubble according to the action it will take when navigating.
     *
     * @see {@link NavigationBubble#CloseTarget}
     */
    static final WritableIntPropertyKey CLOSE_INDICATOR = new WritableIntPropertyKey();

    static final PropertyKey[] ALL_KEYS = {
        ACTION, DIRECTION, EDGE, ACTIVATION_STATUS, BUBBLE_OFFSET, CLOSE_INDICATOR
    };
}
