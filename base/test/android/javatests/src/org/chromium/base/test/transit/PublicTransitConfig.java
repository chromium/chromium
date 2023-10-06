// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import android.widget.Toast;

import androidx.test.platform.app.InstrumentationRegistry;

import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.ThreadUtils;

/** Configuration for PublicTransit tests. */
public class PublicTransitConfig {
    private static final String TAG = "Transit";
    private static long sTransitionPause;

    /**
     * Set a pause for all transitions for debugging.
     *
     * @param millis how long to pause for (1000 to 4000 ms is typical).
     */
    public static void setTransitionPauseForDebugging(long millis) {
        sTransitionPause = millis;
        ResettersForTesting.register(() -> sTransitionPause = 0);
    }

    static void maybePauseAfterTransition(ConditionalState state) {
        long pauseMs = sTransitionPause;
        if (pauseMs > 0) {
            ThreadUtils.runOnUiThread(
                    () -> {
                        Toast.makeText(
                                        InstrumentationRegistry.getInstrumentation()
                                                .getTargetContext(),
                                        state.toString(),
                                        Toast.LENGTH_SHORT)
                                .show();
                    });
            try {
                Log.e(TAG, "Pause for sightseeing %s for %dms", state, pauseMs);
                Thread.sleep(pauseMs);
            } catch (InterruptedException e) {
                Log.e(TAG, "Interrupted pause", e);
            }
        }
    }
}
