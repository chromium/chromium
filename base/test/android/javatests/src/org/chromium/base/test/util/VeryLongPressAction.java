// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.util;

import static org.junit.Assert.assertNotNull;

import android.view.InputDevice;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewConfiguration;

import androidx.test.espresso.UiController;
import androidx.test.espresso.ViewAction;
import androidx.test.espresso.action.GeneralClickAction;
import androidx.test.espresso.action.GeneralLocation;
import androidx.test.espresso.action.MotionEvents;
import androidx.test.espresso.action.Press;
import androidx.test.espresso.action.Tapper;

import org.hamcrest.Matcher;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.Locale;

/**
 * A long press action used when {@link ViewActions#longClick()} does not hold down for long enough.
 */
@NullMarked
public class VeryLongPressAction implements ViewAction {
    private final float mLongPressMultiple;
    private final @Nullable Matcher<View> mConstraints;

    /**
     * @param longPressMultiple The multiple of (@link ViewConfiguration#getLongPressTimeout()} to
     *     long press for.
     * @param constraints The constraints that the View must satisfy for the action to be performed.
     */
    public VeryLongPressAction(float longPressMultiple, @Nullable Matcher<View> constraints) {
        mLongPressMultiple = longPressMultiple;
        mConstraints = constraints;

        assert Math.abs(longPressMultiple - 1.0f) > 0.2f
                : "Avoid values close to the long press threshold to avoid flakiness";
    }

    @Override
    public @Nullable Matcher<View> getConstraints() {
        return mConstraints;
    }

    @Override
    public String getDescription() {
        return String.format(
                Locale.ROOT, "Perform a long press of multiple %f", mLongPressMultiple);
    }

    @Override
    public void perform(UiController uiController, View view) {
        VeryLongTap tap = new VeryLongTap(mLongPressMultiple);
        GeneralClickAction clickAction =
                new GeneralClickAction(
                        tap,
                        GeneralLocation.CENTER,
                        Press.FINGER,
                        InputDevice.SOURCE_UNKNOWN,
                        MotionEvent.BUTTON_PRIMARY);
        clickAction.perform(uiController, view);
    }

    /** An internal implementation of the tapper for the very long press. */
    private static class VeryLongTap implements Tapper {
        private final float mLongPressMultiple;

        /**
         * @param longPressMultiple The multiple of (@link ViewConfiguration#getLongPressTimeout()}
         *     to long press for.
         */
        public VeryLongTap(float longPressMultiple) {
            mLongPressMultiple = longPressMultiple;
        }

        @Override
        public Tapper.Status sendTap(
                UiController uiController, float[] coordinates, float[] precision) {
            return sendTap(uiController, coordinates, precision, 0, 0);
        }

        @Override
        public Tapper.Status sendTap(
                UiController uiController,
                float[] coordinates,
                float[] precision,
                int inputDevice,
                int buttonState) {
            assertNotNull(uiController);
            assertNotNull(coordinates);
            assertNotNull(precision);

            MotionEvent downEvent =
                    MotionEvents.sendDown(
                                    uiController, coordinates, precision, inputDevice, buttonState)
                            .down;
            try {
                long longPressTimeout =
                        (long) (ViewConfiguration.getLongPressTimeout() * mLongPressMultiple);
                uiController.loopMainThreadForAtLeast(longPressTimeout);

                if (!MotionEvents.sendUp(uiController, downEvent)) {
                    MotionEvents.sendCancel(uiController, downEvent);
                    return Tapper.Status.FAILURE;
                }
            } finally {
                downEvent.recycle();
            }
            return Tapper.Status.SUCCESS;
        }
    }
}
