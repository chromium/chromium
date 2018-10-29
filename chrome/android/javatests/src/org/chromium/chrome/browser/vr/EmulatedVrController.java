// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import android.content.Context;
import android.os.SystemClock;
import android.support.annotation.IntDef;

import com.google.vr.testframework.controller.ControllerTestApi;

import org.junit.Assert;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.concurrent.TimeUnit;

/**
 * Wrapper for the ControllerTestApi class to handle more complex actions such
 * as clicking and dragging.
 *
 * Requires that VrCore's settings file is modified to use the test API:
 *   - UseAutomatedController: true
 *   - PairedControllerDriver: "DRIVER_AUTOMATED"
 *   - PairedControllerAddress: "FOO"
 */
public class EmulatedVrController {
    @IntDef({ScrollDirection.UP, ScrollDirection.DOWN, ScrollDirection.LEFT, ScrollDirection.RIGHT})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ScrollDirection {
        int UP = 0;
        int DOWN = 1;
        int LEFT = 2;
        int RIGHT = 3;
    }

    private static final int FIRST_INPUT_DELAY_MS = 1000;
    private final ControllerTestApi mApi;
    private boolean mHaveSentInputSinceEnteringVr;

    public EmulatedVrController(Context context) {
        mApi = new ControllerTestApi(context);
    }

    public ControllerTestApi getApi() {
        // We flakily disconnect and reconnect the controller immediately after VR entry, which can
        // cause controller input to get eaten. Sleeping a bit after VR entry fixes this, so instead
        // of adding a bunch of sleeps everywhere, sleep for a bit before sending the first input
        // after VR entry.
        // TODO(https://crbug.com/870031): Remove this sleep if/when the controller disconnect/
        // reconnect issue caused by DON flow skipping that flakily eats controller input is
        // resolved.
        if (!mHaveSentInputSinceEnteringVr) {
            SystemClock.sleep(FIRST_INPUT_DELAY_MS);
            mHaveSentInputSinceEnteringVr = true;
        }
        return mApi;
    }

    /**
     * Resets the flag used to keep track of whether we have sent input since entering VR. Should
     * be called anytime the emulated controller is used and a test re-enters VR.
     */
    public void resetFirstInputFlag() {
        mHaveSentInputSinceEnteringVr = false;
    }

    /**
     * Touch and release the touchpad to perform a controller click.
     */
    public void performControllerClick() {
        // pressReleaseTouchpadButton() appears to be flaky for clicking on things, as sometimes
        // it happens too fast for Chrome to register. So, manually press and release with a delay
        sendClickButtonToggleEvent();
        SystemClock.sleep(50);
        sendClickButtonToggleEvent();
    }

    /**
     * Either presses or releases the Daydream controller's touchpad button depending on wheter
     * the button is currently pressed or not.
     */
    public void sendClickButtonToggleEvent() {
        getApi().buttonEvent.sendClickButtonToggleEvent();
    }

    /**
     * Presses and quickly releases the Daydream controller's touchpad button.
     * Or, if the button is already pressed, releases and quickly presses again.
     */
    public void pressReleaseTouchpadButton() {
        getApi().buttonEvent.sendClickButtonEvent();
    }

    /**
     * Presses and quickly releases the Daydream controller's app button.
     * Or, if the button is already pressed, releases and quickly presses again.
     */
    public void pressReleaseAppButton() {
        getApi().buttonEvent.sendAppButtonEvent();
    }

    /**
     * Holds the home button to recenter the view using an arbitrary, but valid
     * orientation quaternion.
     */
    public void recenterView() {
        getApi().buttonEvent.sendHomeButtonToggleEvent();
        // Need to "hold" the button long enough to trigger a view recenter instead of just a button
        // press - half a second is sufficient and non-flaky.
        SystemClock.sleep(500);
        getApi().buttonEvent.sendHomeButtonToggleEvent();
    }

    /**
     * Performs a short home button press/release, which launches the Daydream Home app.
     */
    public void goToDaydreamHome() {
        getApi().buttonEvent.sendShortHomeButtonEvent();
    }

    /**
     * Performs an swipe on the touchpad in order to scroll in the specified
     * direction while in the VR browser.
     * Note that scrolling this way is not consistent, i.e. scrolling down then
     * scrolling up at the same speed won't necessarily scroll back to the exact
     * starting position on the page.
     *
     * @param direction the ScrollDirection to scroll with.
     * @param steps the number of intermediate steps to send while scrolling.
     * @param speed how long to wait between steps in the scroll, with higher
     *        numbers resulting in a faster scroll.
     */
    public void scroll(@ScrollDirection int direction, int steps, int speed) {
        float startX, startY, endX, endY;
        startX = startY = endX = endY = 0.5f;
        switch (direction) {
            case ScrollDirection.UP:
                startY = 0.1f;
                endY = 0.9f;
                break;
            case ScrollDirection.DOWN:
                startY = 0.9f;
                endY = 0.1f;
                break;
            case ScrollDirection.LEFT:
                startX = 0.1f;
                endX = 0.9f;
                break;
            case ScrollDirection.RIGHT:
                startX = 0.9f;
                endX = 0.1f;
                break;
            default:
                Assert.fail("Unknown scroll direction enum given");
        }
        performLinearTouchpadMovement(startX, startY, endX, endY, steps, speed);
    }

    /**
     * Touches then releases the touchpad to cancel fling scroll.
     */
    public void cancelFlingScroll() {
        // Arbitrary amount of delay to both ensure that the touchpad press is properly registered
        // and long enough that we don't accidentally trigger any functionality bound to quick
        // touchpad taps if there is any.
        int delay = 500;
        long simulatedDelay = TimeUnit.MILLISECONDS.toNanos(delay);
        long timestamp = mApi.touchEvent.startTouchSequence(0.5f, 0.5f, simulatedDelay, delay);
        getApi().touchEvent.endTouchSequence(0.5f, 0.5f, timestamp, simulatedDelay, delay);
    }

    /**
     * Simulates a touch down-drag-touch up sequence on the touchpad between two points.
     *
     * @param xStart the x coordinate to start the touch sequence at, in range [0.0f, 1.0f].
     * @param yStart the y coordinate to start the touch sequence at, in range [0.0f, 1.0f].
     * @param xEnd the x coordinate to end the touch sequence at, in range [0.0f, 1.0f].
     * @param yEnd the y coordinate to end the touch sequence at, in range [0.0f, 1.0f].
     * @param steps the number of steps the drag will have.
     * @param speed how long to wait between steps in the sequence. Generally, higher numbers
     *        result in faster movement, e.g. when used for scrolling, a higher number results in
     *        faster scrolling.
     */
    public void performLinearTouchpadMovement(
            float xStart, float yStart, float xEnd, float yEnd, int steps, int speed) {
        // Touchpad events have timestamps attached to them in nanoseconds - for smooth scrolling,
        // the timestamps should increase at a similar rate to the amount of time we actually wait
        // between sending events, which is determined by the given speed.
        long simulatedDelay = TimeUnit.MILLISECONDS.toNanos(speed);
        long timestamp = mApi.touchEvent.startTouchSequence(xStart, yStart, simulatedDelay, speed);
        timestamp = mApi.touchEvent.dragFromTo(
                xStart, yStart, xEnd, yEnd, steps, timestamp, simulatedDelay, speed);
        getApi().touchEvent.endTouchSequence(xEnd, yEnd, timestamp, simulatedDelay, speed);
    }

    /**
     * Instantly moves the controller to the specified quaternion coordinates.
     *
     * @param x the x component of the quaternion.
     * @param y the y component of the quaternion.
     * @param z the z component of the quaternion.
     * @param w the w component of the quaternion.
     */
    public void moveControllerInstant(float x, float y, float z, float w) {
        getApi().moveEvent.sendMoveEvent(x, y, z, w, 0);
    }

    /**
     * Moves the controller from one position to another over a period of time.
     *
     * @param startAngles the x/y/z angles to start the controller at, in radians.
     * @param endAngles the x/y/z angles to end the controller at, in radians.
     * @param steps the number of steps the controller will take moving between the positions.
     * @param delayBetweenSteps how long to sleep between positions.
     */
    public void moveControllerInterpolated(
            float[] startAngles, float[] endAngles, int steps, int delayBetweenSteps) {
        if (startAngles.length != 3 || endAngles.length != 3) {
            throw new IllegalArgumentException("Angle arrays must be length 3");
        }
        getApi().moveEvent.sendMoveEvent(new float[] {startAngles[0], endAngles[0]},
                new float[] {startAngles[1], endAngles[1]},
                new float[] {startAngles[2], endAngles[2]}, steps, delayBetweenSteps);
    }
}
