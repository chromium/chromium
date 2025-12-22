// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import androidx.test.espresso.ViewAction;
import androidx.test.espresso.ViewAssertion;
import androidx.test.espresso.ViewInteraction;

import org.chromium.build.annotations.NullMarked;

/**
 * Provides ways for tests to interact with a View.
 *
 * <p>Implemented by {@link ViewElement}, {@link OptionalViewElement} and {@link ViewCarryOn}.
 */
@NullMarked
public interface ViewInterface {
    /** Start a Transition by clicking this View. */
    TripBuilder clickTo();

    /** Start a Transition by long pressing this View. */
    TripBuilder longPressTo();

    /** Start a Transition by typing |text| into this View char by char. */
    TripBuilder typeTextTo(String text);

    /** Start a Transition by performing an Espresso ViewAction on this View. */
    TripBuilder performViewActionTo(ViewAction action);

    /** Click this View waiting only for the UI Thread to be idle afterwards. */
    default void click() {
        clickTo().executeTriggerWithoutTransition();
    }

    /** Long press this View waiting only for the UI Thread to be idle afterwards. */
    default void longPress() {
        longPressTo().executeTriggerWithoutTransition();
    }

    /** Type text into this View waiting only for the UI Thread to be idle afterwards. */
    default void typeText(String text) {
        typeTextTo(text).executeTriggerWithoutTransition();
    }

    /**
     * Perform an Espresso ViewAction on this View waiting only for the UI Thread to be idle
     * afterwards.
     */
    default void performViewAction(ViewAction action) {
        performViewActionTo(action).executeTriggerWithoutTransition();
    }

    /** Trigger an Espresso ViewAssertion on this View. */
    void check(ViewAssertion assertion);

    /**
     * @deprecated Use the trigger methods instead, such as {@link #performViewActionTo(ViewAction)}
     *     and {@link #clickTo()}.
     */
    @Deprecated
    ViewInteraction onView();
}
