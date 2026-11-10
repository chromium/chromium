// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import android.app.Activity;

import org.chromium.build.annotations.NullMarked;

/**
 * The first station in all Public Transit tests.
 *
 * <p>No Transition is made to enter this Station; it's a sentinel for the first transition to
 * happen from a non-null origin Station.
 */
@NullMarked
public class EntryPointSentinelStation extends Station<Activity> {
    /**
     * Default timeout for the initial transition out of {@link EntryPointSentinelStation}.
     *
     * <p>Cold-starting the instrumentation process and launching the initial Activity synchronously
     * inside the entry trigger can exceed {@link ConditionWaiter#MAX_TIME_TO_POLL} before
     * destination Conditions are polled.
     */
    public static final long ENTRY_POINT_TIMEOUT_MS = 12000L;

    public EntryPointSentinelStation() {
        super(null);
    }

    /**
     * Set this station as the entry point for Public Transit tests. Only EntryPoints should call
     * this. Will assert if called twice.
     */
    public void setAsEntryPoint() {
        setStateActiveWithoutTransition();
        TrafficControl.notifyEntryPointSentinelStationCreated(this);
    }

    @Override
    public TripBuilder noopTo() {
        return super.noopTo().withTimeout(ENTRY_POINT_TIMEOUT_MS);
    }

    @Override
    public TripBuilder runTo(Runnable runnable) {
        return super.runTo(runnable).withTimeout(ENTRY_POINT_TIMEOUT_MS);
    }

    @Override
    public TripBuilder runOnUiThreadTo(Runnable runnable) {
        return super.runOnUiThreadTo(runnable).withTimeout(ENTRY_POINT_TIMEOUT_MS);
    }
}
