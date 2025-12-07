// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import androidx.test.espresso.Espresso;

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
     * Press back to trigger a Transition.
     *
     * @return a {@link TripBuilder} to perform the Transition.
     */
    public static TripBuilder pressBackTo() {
        return new TripBuilder().withTrigger(Espresso::pressBack);
    }
}
