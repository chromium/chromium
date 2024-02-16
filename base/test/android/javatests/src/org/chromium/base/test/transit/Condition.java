// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

/**
 * A condition that needs to be fulfilled for a state transition to be considered done.
 *
 * <p>{@link ConditionWaiter} waits for multiple Conditions to be fulfilled. {@link
 * ConditionChecker} performs one-time checks for whether multiple Conditions are fulfilled.
 */
public abstract class Condition {
    private String mDescription;

    private boolean mIsRunOnUiThread;

    /**
     * @param isRunOnUiThread true if the Condition should be checked on the UI Thread, false if it
     *     should be checked on the Instrumentation Thread.
     */
    public Condition(boolean isRunOnUiThread) {
        mIsRunOnUiThread = isRunOnUiThread;
    }

    /**
     * Called on the instrumentation thread, depending on #shouldRunOnUiThread().
     *
     * @return whether the condition has been fulfilled.
     */
    public abstract boolean check() throws Exception;

    /**
     * @return a short description to be printed as part of a list of conditions. Use {@link
     *     #getDescription()} to get a description as it caches the description until {@link
     *     #rebuildDescription()} invalidates it.
     */
    public abstract String buildDescription();

    /**
     * Hook run right before the condition starts being monitored. Used, for example, to get initial
     * callback counts.
     */
    public void onStartMonitoring() {}

    /**
     * @return a short description to be printed as part of a list of conditions.
     */
    public String getDescription() {
        if (mDescription == null) {
            rebuildDescription();
        }
        return mDescription;
    }

    /**
     * Invalidates last description; the next time {@link #getDescription()}, it will get a new one
     * from {@link #buildDescription()}.
     */
    protected void rebuildDescription() {
        mDescription = buildDescription();
    }

    /**
     * @return true if the check is intended to be run on the UI Thread, false if it should be run
     *     on the instrumentation thread.
     */
    public boolean isRunOnUiThread() {
        return mIsRunOnUiThread;
    }
}
