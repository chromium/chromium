// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import android.app.Activity;

import com.google.errorprone.annotations.CanIgnoreReturnValue;

import org.chromium.base.test.transit.ConditionalState.Phase;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * The state-changes half of a Trip: a Station to arrive at, Facilities and States to enter or exit,
 * and Conditions to wait for.
 *
 * <p>A {@link TripBuilder} is the other half: the Trigger and its options.
 */
@NullMarked
class Outcome {
    private final List<Facility<?>> mFacilitiesToEnter = new ArrayList<>();
    private final List<Facility<?>> mFacilitiesToExit = new ArrayList<>();
    private final List<State> mStatesToEnter = new ArrayList<>();
    private final List<State> mStatesToExit = new ArrayList<>();
    private final List<Condition> mConditions = new ArrayList<>();
    private @Nullable Station<?> mDestination;
    private @Nullable Activity mHopOnActivity;
    private boolean mInNewTask;

    /** Also enter |facilities|. */
    @CanIgnoreReturnValue
    Outcome and(Facility<?>... facilities) {
        for (Facility<?> facility : facilities) {
            facility.assertInPhase(Phase.NEW);
            mFacilitiesToEnter.add(facility);
        }
        return this;
    }

    /** Also enter |state|. */
    @CanIgnoreReturnValue
    Outcome and(State state) {
        state.assertInPhase(Phase.NEW);
        mStatesToEnter.add(state);
        return this;
    }

    /** Also exit |facilities|. */
    @CanIgnoreReturnValue
    Outcome andExit(Facility<?>... facilities) {
        mFacilitiesToExit.addAll(Arrays.asList(facilities));
        return this;
    }

    /** Also exit |state|. */
    @CanIgnoreReturnValue
    Outcome andExit(State state) {
        mStatesToExit.add(state);
        return this;
    }

    /** Also wait for |conditions|. */
    @CanIgnoreReturnValue
    Outcome andWaitFor(Condition... conditions) {
        mConditions.addAll(Arrays.asList(conditions));
        return this;
    }

    /** Set the Station to arrive at, or to hop on at if |hopOnActivity| is not null. */
    void setDestination(Station<?> destination, @Nullable Activity hopOnActivity) {
        assert mDestination == null : "Destination already set to " + mDestination.getName();
        assert hopOnActivity == null || !mInNewTask
                : "Hopping on cannot be combined with a new task";
        destination.assertInPhase(Phase.NEW);
        mDestination = destination;
        mHopOnActivity = hopOnActivity;
    }

    /** The destination Station is in a new task, so the Stations already active are not exited. */
    void setInNewTask() {
        mInNewTask = true;
    }

    @Nullable Station<?> getDestination() {
        return mDestination;
    }

    @Nullable Activity getHopOnActivity() {
        return mHopOnActivity;
    }

    List<Facility<?>> getFacilitiesToEnter() {
        return mFacilitiesToEnter;
    }

    List<Facility<?>> getFacilitiesToExit() {
        return mFacilitiesToExit;
    }

    List<State> getStatesToEnter() {
        return mStatesToEnter;
    }

    List<State> getStatesToExit() {
        return mStatesToExit;
    }

    List<Condition> getConditions() {
        return mConditions;
    }

    boolean isInNewTask() {
        return mInNewTask;
    }
}
