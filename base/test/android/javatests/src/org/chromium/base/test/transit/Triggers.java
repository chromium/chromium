// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import android.content.Intent;

import androidx.test.espresso.Espresso;
import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.uiautomator.UiDevice;

import org.chromium.base.ContextUtils;

/** Common Triggers to start Transitions. */
public class Triggers {
    /**
     * Do nothing to trigger a Transition.
     *
     * <p>This is useful to wait ConditionalStates/Conditions to be reached without performing any
     * action.
     */
    public static TripBuilder noopTo() {
        return new TripBuilder().withTrigger(Transition.NOOP_TRIGGER);
    }

    /**
     * Run an arbitrary function on the instrumentation thread to trigger a Transition.
     *
     * @return a {@link TripBuilder} to perform the Transition.
     */
    public static TripBuilder runTo(Runnable runnable) {
        return new TripBuilder().withRunnableTrigger(runnable);
    }

    /**
     * Run an arbitrary function on the UI thread to trigger a Transition.
     *
     * @return a {@link TripBuilder} to perform the Transition.
     */
    public static TripBuilder runOnUiThreadTo(Runnable runnable) {
        return new TripBuilder().withRunnableTrigger(runnable).withRunOnUiThread();
    }

    /**
     * Start an Activity with an Intent from the application context to trigger a Transition.
     *
     * <p>The Intent needs {@link Intent#FLAG_ACTIVITY_NEW_TASK}, since it is started from the
     * application context.
     *
     * @return a {@link TripBuilder} to perform the Transition.
     */
    public static TripBuilder startActivityTo(Intent intent) {
        assert (intent.getFlags() & Intent.FLAG_ACTIVITY_NEW_TASK) != 0
                : "Starting an Activity from the application context requires"
                        + " FLAG_ACTIVITY_NEW_TASK";
        return runTo(() -> ContextUtils.getApplicationContext().startActivity(intent));
    }

    /**
     * Press back to trigger a Transition.
     *
     * @return a {@link TripBuilder} to perform the Transition.
     */
    public static TripBuilder pressBackTo() {
        return new TripBuilder()
                .withTrigger(
                        () -> {
                            try {
                                Espresso.pressBack();
                            } catch (IllegalStateException e) {
                                if (e.getMessage() != null
                                        && e.getMessage()
                                                .contains(
                                                        "There is not a single resumed Activity")) {
                                    // Espresso.pressBack() fails in multi-window environments (e.g.
                                    // Desktop Android or split-screen) when multiple activities are
                                    // in Stage.RESUMED simultaneously.
                                    UiDevice.getInstance(
                                                    InstrumentationRegistry.getInstrumentation())
                                            .pressBack();
                                } else {
                                    throw e;
                                }
                            }
                        });
    }
}
