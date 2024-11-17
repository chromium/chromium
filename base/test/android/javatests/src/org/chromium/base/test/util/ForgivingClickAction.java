// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.util;

import static org.hamcrest.CoreMatchers.instanceOf;

import android.view.InputDevice;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewConfiguration;
import android.webkit.WebView;

import androidx.test.espresso.PerformException;
import androidx.test.espresso.UiController;
import androidx.test.espresso.ViewAction;
import androidx.test.espresso.action.CoordinatesProvider;
import androidx.test.espresso.action.GeneralLocation;
import androidx.test.espresso.action.PrecisionDescriber;
import androidx.test.espresso.action.Press;
import androidx.test.espresso.action.Tap;
import androidx.test.espresso.action.Tapper;
import androidx.test.espresso.util.HumanReadables;

import org.hamcrest.Matcher;

import org.chromium.base.Log;

import java.util.Locale;

/**
 * A ViewAction to click a View, regardless of how much of it is displayed.
 *
 * <p>Based on GeneralClickAction, cannot extend to remove displayed% constraint since it's final.
 */
public class ForgivingClickAction implements ViewAction {
    private static final String TAG = "ForgivingClickAction";

    private final CoordinatesProvider mCoordinatesProvider;
    private final Tapper mTapper;
    private final PrecisionDescriber mPrecisionDescriber;
    private final int mInputDevice;
    private final int mButtonState;

    public static ForgivingClickAction forgivingClick() {
        return new ForgivingClickAction(
                Tap.SINGLE,
                GeneralLocation.VISIBLE_CENTER,
                Press.FINGER,
                InputDevice.SOURCE_UNKNOWN,
                MotionEvent.BUTTON_PRIMARY);
    }

    public ForgivingClickAction(
            Tapper tapper,
            CoordinatesProvider coordinatesProvider,
            PrecisionDescriber precisionDescriber,
            int inputDevice,
            int buttonState) {
        this.mCoordinatesProvider = coordinatesProvider;
        this.mTapper = tapper;
        this.mPrecisionDescriber = precisionDescriber;
        this.mInputDevice = inputDevice;
        this.mButtonState = buttonState;
    }

    @Override
    public String getDescription() {
        return mTapper.toString().toLowerCase() + " click";
    }

    @Override
    public Matcher<View> getConstraints() {
        return instanceOf(View.class);
    }

    @Override
    public void perform(UiController uiController, View view) {
        float[] coordinates = mCoordinatesProvider.calculateCoordinates(view);
        float[] precision = mPrecisionDescriber.describePrecision();

        Tapper.Status status;
        try {
            status =
                    mTapper.sendTap(
                            uiController, coordinates, precision, mInputDevice, mButtonState);
            Log.d(
                    TAG,
                    "perform: "
                            + String.format(
                                    Locale.ROOT,
                                    "%s - At Coordinates: %d, %d and precision: %d, %d",
                                    this.getDescription(),
                                    (int) coordinates[0],
                                    (int) coordinates[1],
                                    (int) precision[0],
                                    (int) precision[1]));
        } catch (RuntimeException re) {
            throw new PerformException.Builder()
                    .withActionDescription(
                            String.format(
                                    Locale.ROOT,
                                    "%s - At Coordinates: %d, %d and precision: %d, %d",
                                    this.getDescription(),
                                    (int) coordinates[0],
                                    (int) coordinates[1],
                                    (int) precision[0],
                                    (int) precision[1]))
                    .withViewDescription(HumanReadables.describe(view))
                    .withCause(re)
                    .build();
        }

        int duration = ViewConfiguration.getPressedStateDuration();
        // ensures that all work enqueued to process the tap has been run.
        if (duration > 0) {
            uiController.loopMainThreadForAtLeast(duration);
        }

        if (status == Tapper.Status.FAILURE) {
            throw new PerformException.Builder()
                    .withActionDescription(this.getDescription())
                    .withViewDescription(HumanReadables.describe(view))
                    .withCause(
                            new RuntimeException(
                                    String.format(
                                            Locale.ROOT,
                                            "Couldn't click at: %s,%s precision: %s, %s . Tapper:"
                                                + " %s coordinate provider: %s precision describer:"
                                                + " %s.",
                                            coordinates[0],
                                            coordinates[1],
                                            precision[0],
                                            precision[1],
                                            mTapper,
                                            mCoordinatesProvider,
                                            mPrecisionDescriber)))
                    .build();
        }

        if (mTapper == Tap.SINGLE && view instanceof WebView) {
            // WebViews will not process click events until double tap
            // timeout. Not the best place for this - but good for now.
            uiController.loopMainThreadForAtLeast(ViewConfiguration.getDoubleTapTimeout());
        }
    }
}
