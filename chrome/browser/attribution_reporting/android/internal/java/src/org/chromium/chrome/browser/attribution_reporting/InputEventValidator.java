// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.attribution_reporting;

import android.content.Context;
import android.hardware.input.InputManager;
import android.os.Build;
import android.os.SystemClock;
import android.view.InputEvent;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.VerifiedInputEvent;
import android.view.VerifiedKeyEvent;
import android.view.VerifiedMotionEvent;

import org.chromium.base.ContextUtils;
import org.chromium.base.Predicate;
import org.chromium.base.compat.ApiHelperForR;

/**
 * Validates input events for Attribution Reporting, using InputManager#verifyInputEvent
 * on Android R+.
 */
public class InputEventValidator implements Predicate<InputEvent> {
    private static final long NANOS_PER_MILLISECOND = 1000000;

    // 10 second expiry time for InputEvents to allow for slow devices having to launch Chrome.
    /* package */ static final long INPUT_EXPIRY_MILLIS = 10 * 1000;

    // In SystemClock#uptimeMillis time base, in nanos.
    private long mLastEventDowntime;

    @Override
    public boolean test(InputEvent inputEvent) {
        // We cannot verify input events pre-R, so we're making a trade-off of compat vs. security
        // by allowing un-verified input pre-R.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) return true;

        InputManager im = (InputManager) ContextUtils.getApplicationContext().getSystemService(
                Context.INPUT_SERVICE);

        VerifiedInputEvent verifiedEvent = ApiHelperForR.verifyInputEvent(im, inputEvent);
        if (verifiedEvent == null) return false;

        // Per documentation, EventTimeNanos is in the SystemClock.uptimeMillis() time base, just
        // with nanosecond precision (but likely not nanosecond accuracy).
        long eventTimeMillis = verifiedEvent.getEventTimeNanos() / NANOS_PER_MILLISECOND;
        if (SystemClock.uptimeMillis() - eventTimeMillis > INPUT_EXPIRY_MILLIS) return false;

        long eventDownTime;
        if (verifiedEvent instanceof VerifiedMotionEvent) {
            VerifiedMotionEvent motionEvent = (VerifiedMotionEvent) verifiedEvent;

            // Only allow ACTION_UP to be more equivalent to User Activitation on the web and
            // prevent canceled events from triggering attribution.
            if (motionEvent.getActionMasked() != MotionEvent.ACTION_UP) return false;
            eventDownTime = motionEvent.getDownTimeNanos();
        } else if (verifiedEvent instanceof VerifiedKeyEvent) {
            VerifiedKeyEvent keyEvent = (VerifiedKeyEvent) verifiedEvent;
            if (keyEvent.getAction() != KeyEvent.ACTION_UP) return false;
            eventDownTime = keyEvent.getDownTimeNanos();
        } else {
            // Not reachable as of API level 30.
            assert false;
            return false;
        }

        // In order to avoid event re-use, make sure that the input sequence/gesture that triggered
        // this attribution is newer than the last event to have triggered an attribution.
        if (eventDownTime <= mLastEventDowntime) return false;
        mLastEventDowntime = eventDownTime;
        return true;
    }
}
