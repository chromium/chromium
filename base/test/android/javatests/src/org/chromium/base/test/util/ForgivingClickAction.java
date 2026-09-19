// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.util;

import static org.hamcrest.CoreMatchers.instanceOf;
import static org.hamcrest.Matchers.allOf;

import android.view.InputDevice;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewConfiguration;
import android.webkit.WebView;

import androidx.annotation.Nullable;
import androidx.test.espresso.AppNotIdleException;
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
    private static final String MOTION_EVENTS_CLASS = "androidx.test.espresso.action.MotionEvents";

    private final CoordinatesProvider mCoordinatesProvider;
    private final Tapper mTapper;
    private final PrecisionDescriber mPrecisionDescriber;
    private final int mInputDevice;
    private final int mButtonState;
    @Nullable private final ViewAction mRollbackAction;

    public static ForgivingClickAction forgivingClick() {
        return new ForgivingClickAction(
                Tap.SINGLE,
                GeneralLocation.VISIBLE_CENTER,
                Press.FINGER,
                InputDevice.SOURCE_UNKNOWN,
                MotionEvent.BUTTON_PRIMARY);
    }

    public static ForgivingClickAction forgivingClick(@Nullable ViewAction rollbackAction) {
        return new ForgivingClickAction(
                Tap.SINGLE,
                GeneralLocation.VISIBLE_CENTER,
                Press.FINGER,
                InputDevice.SOURCE_UNKNOWN,
                MotionEvent.BUTTON_PRIMARY,
                rollbackAction);
    }

    public static ForgivingClickAction forgivingLongClick() {
        return new ForgivingClickAction(
                Tap.LONG,
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
        this(tapper, coordinatesProvider, precisionDescriber, inputDevice, buttonState, null);
    }

    public ForgivingClickAction(
            Tapper tapper,
            CoordinatesProvider coordinatesProvider,
            PrecisionDescriber precisionDescriber,
            int inputDevice,
            int buttonState,
            @Nullable ViewAction rollbackAction) {
        this.mCoordinatesProvider = coordinatesProvider;
        this.mTapper = tapper;
        this.mPrecisionDescriber = precisionDescriber;
        this.mInputDevice = inputDevice;
        this.mButtonState = buttonState;
        this.mRollbackAction = rollbackAction;
    }

    @Override
    public String getDescription() {
        if (mTapper == Tap.SINGLE) {
            return "click";
        } else if (mTapper == Tap.LONG) {
            return "long click";
        } else {
            return mTapper.toString().toLowerCase() + " click";
        }
    }

    @Override
    public Matcher<View> getConstraints() {
        Matcher<View> standardConstraint = instanceOf(View.class);
        if (mRollbackAction != null) {
            return allOf(standardConstraint, mRollbackAction.getConstraints());
        }
        return standardConstraint;
    }

    @Override
    public void perform(UiController uiController, View view) {
        float[] coordinates = mCoordinatesProvider.calculateCoordinates(view);
        float[] precision = mPrecisionDescriber.describePrecision();

        Tapper.Status status = Tapper.Status.FAILURE;
        int loopCount = 0;
        while (status != Tapper.Status.SUCCESS && loopCount < 3) {
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
                if (isAppNotIdleExceptionAfterEventsSent(re)) {
                    Log.w(
                            TAG,
                            "Main looper never went idle after "
                                    + getDescription()
                                    + "; the tap was already delivered, so treating it as"
                                    + " successful and letting the caller's own conditions decide"
                                    + " whether the UI reacted.",
                            re);
                    status = Tapper.Status.SUCCESS;
                    break;
                }
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
                loopMainThreadForAtLeastForgivingly(uiController, duration);
            }

            if (status == Tapper.Status.WARNING) {
                if (mRollbackAction != null) {
                    mRollbackAction.perform(uiController, view);
                } else {
                    break;
                }
            }
            loopCount++;
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
                                                + " %s. Tried %d times. With Rollback? %b",
                                            coordinates[0],
                                            coordinates[1],
                                            precision[0],
                                            precision[1],
                                            mTapper,
                                            mCoordinatesProvider,
                                            mPrecisionDescriber,
                                            loopCount,
                                            mRollbackAction != null)))
                    .build();
        }

        if (mTapper == Tap.SINGLE && view instanceof WebView) {
            // WebViews will not process click events until double tap
            // timeout. Not the best place for this - but good for now.
            loopMainThreadForAtLeastForgivingly(
                    uiController, ViewConfiguration.getDoubleTapTimeout());
        }
    }

    /**
     * Waits for at least {@code duration} ms, tolerating the main looper never going idle.
     *
     * <p>{@link UiController#loopMainThreadForAtLeast(long)} ends with loopMainThreadUntilIdle(),
     * so it throws {@link AppNotIdleException} under a continuously animating UI. The tap has
     * already been delivered by the time this is called, so settling is best-effort.
     */
    private static void loopMainThreadForAtLeastForgivingly(
            UiController uiController, int duration) {
        try {
            uiController.loopMainThreadForAtLeast(duration);
        } catch (AppNotIdleException e) {
            Log.w(TAG, "Main looper did not settle after the tap; continuing anyway.", e);
        }
    }

    /**
     * Returns whether {@code t} is an {@link AppNotIdleException} raised after the tap's UP event
     * was already injected.
     *
     * <p>Espresso's UiControllerImpl#injectMotionEvent() submits the injection task and calls
     * loopUntil(MOTION_INJECTION_HAS_COMPLETED). Once the event is injected and signaled,
     * Interrogator only checks whether conditions are met when the queue is empty or the head
     * message is due more than 15ms out (taskDueLong); while a sync barrier is up or the head
     * message is due within 15ms (taskDueSoon, e.g. 60Hz frame callbacks every 16.6ms), it keeps
     * looping and eventually fails with MAIN_LOOPER_HAS_IDLED. An AppNotIdleException for
     * MAIN_LOOPER_HAS_IDLED raised underneath MotionEvents#sendUp() therefore means the whole
     * DOWN/UP pair was delivered and only the post-injection quiescence check failed. Callers
     * verify the resulting UI state themselves (Public Transit does so via its arrival Conditions),
     * so the tap can be reported as successful.
     *
     * <p>Exceptions raised before the UP event are not forgiven: the gesture would be left open,
     * with no UP or CANCEL, free to decay into a spurious long press.
     */
    private boolean isAppNotIdleExceptionAfterEventsSent(Throwable t) {
        // Walking the cause chain is defensive; no Espresso path currently wraps
        // AppNotIdleException, but nothing guarantees that stays true.
        while (t != null) {
            if (t instanceof AppNotIdleException) {
                String message = t.getMessage();
                return message != null
                        && message.contains("MAIN_LOOPER_HAS_IDLED")
                        && isRaisedAfterUpEvent(t);
            }
            t = t.getCause();
        }
        return false;
    }

    private boolean isRaisedAfterUpEvent(Throwable t) {
        if (mTapper == Tap.DOUBLE) {
            return false;
        }
        if (hasStackFrame(t, MOTION_EVENTS_CLASS, "sendUp")) {
            return true;
        }
        // Tap.SINGLE waits a further 1.5 * tapTimeout after sendUp() has returned. That wait has
        // neither sendUp() nor sendDown() on the stack, yet both events are already out, so the
        // absence of a sendDown() frame is enough to conclude the tap completed. Tap.LONG cannot
        // use the same deduction: it holds the press by waiting *before* calling sendUp(), so an
        // exception with no sendUp() frame may well mean the UP was never sent.
        if (mTapper != Tap.SINGLE) {
            return false;
        }
        return !hasStackFrame(t, MOTION_EVENTS_CLASS, "sendDown")
                && !hasStackFrame(t, MOTION_EVENTS_CLASS, "sendCancel");
    }

    private static boolean hasStackFrame(Throwable t, String className, String methodName) {
        for (StackTraceElement frame : t.getStackTrace()) {
            if (frame.getClassName().equals(className)
                    && frame.getMethodName().equals(methodName)) {
                return true;
            }
        }
        return false;
    }
}
