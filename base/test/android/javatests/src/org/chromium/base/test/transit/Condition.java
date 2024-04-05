// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import androidx.annotation.Nullable;

import com.google.errorprone.annotations.FormatMethod;

import org.chromium.base.test.transit.ConditionStatus.Status;

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
     * Should check the condition, report its status (if useful) and return whether it is fulfilled.
     *
     * <p>Depending on #shouldRunOnUiThread(), called on the UI or the instrumentation thread.
     *
     * @return {@link ConditionStatus} stating whether the condition has been fulfilled and
     *     optionally more details about its state.
     */
    public abstract ConditionStatus check() throws Exception;

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

    /** {@link #check()} should return this when a Condition is fulfilled. */
    public static ConditionStatus fulfilled() {
        return fulfilled(/* message= */ null);
    }

    /** {@link #fulfilled()} with more details to be logged as a short message. */
    public static ConditionStatus fulfilled(@Nullable String message) {
        return new ConditionStatus(Status.FULFILLED, message);
    }

    /** {@link #fulfilled()} with more details to be logged as a short message. */
    @FormatMethod
    public static ConditionStatus fulfilled(String message, Object... args) {
        return new ConditionStatus(Status.FULFILLED, String.format(message, args));
    }

    /** {@link #check()} should return this when a Condition is not fulfilled. */
    public static ConditionStatus notFulfilled() {
        return notFulfilled(/* message= */ null);
    }

    /** {@link #notFulfilled()} with more details to be logged as a short message. */
    public static ConditionStatus notFulfilled(@Nullable String message) {
        return new ConditionStatus(Status.NOT_FULFILLED, message);
    }

    /** {@link #notFulfilled()} with more details to be logged as a short message. */
    @FormatMethod
    public static ConditionStatus notFulfilled(String message, Object... args) {
        return new ConditionStatus(Status.NOT_FULFILLED, String.format(message, args));
    }

    /**
     * {@link #check()} should return this when an error happens while checking a Condition.
     *
     * <p>A short message is required.
     *
     * <p>Throwing an error in check() has the same effect.
     */
    public static ConditionStatus error(@Nullable String message) {
        return new ConditionStatus(Status.ERROR, message);
    }

    /** {@link #error(String)} with format parameters. */
    @FormatMethod
    public static ConditionStatus error(String message, Object... args) {
        return new ConditionStatus(Status.ERROR, String.format(message, args));
    }

    /** {@link #check()} should return this as a convenience method. */
    public static ConditionStatus whether(boolean isFulfilled) {
        return isFulfilled ? fulfilled() : notFulfilled();
    }

    /** {@link #whether(boolean)} with more details to be logged as a short message. */
    public static ConditionStatus whether(boolean isFulfilled, @Nullable String message) {
        return isFulfilled ? fulfilled(message) : notFulfilled(message);
    }

    /** {@link #whether(boolean)} with more details to be logged as a short message. */
    @FormatMethod
    public static ConditionStatus whether(boolean isFulfilled, String message, Object... args) {
        return whether(isFulfilled, String.format(message, args));
    }
}
