// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import androidx.annotation.Nullable;

import org.chromium.base.Log;

import java.util.ArrayList;

/**
 * A {@link Transition} into a {@link TransitStation}, either from another TransitStation or as an
 * entry point.
 */
public class Trip extends Transition {
    private static final String TAG = "Transit";
    private final int mId;

    @Nullable private final TransitStation mOrigin;
    private final TransitStation mDestination;

    private static int sLastTripId;

    private Trip(@Nullable TransitStation origin, TransitStation destination, Trigger trigger) {
        super(trigger);
        mOrigin = origin;
        mDestination = destination;
        mId = ++sLastTripId;
    }

    /**
     * Starts a transition from a TransitStation to another (or from no TransitStation if at an
     * entry point). Runs the transition |trigger|, and blocks until the destination TransitStation
     * is considered ACTIVE (enter Conditions are fulfilled), the origin TransitStation is
     * considered FINISHED (exit Conditions are fulfilled), and the Transition's conditions are
     * fulfilled.
     *
     * @param origin the StationFacility to depart from, null if at an entry point.
     * @param destination the StationFacility to arrive at.
     * @param trigger the trigger to start the transition (e.g. clicking a view).
     * @return the TransitStation entered.
     * @param <T> the type of TransitStation entered.
     */
    public static <T extends TransitStation> T travelSync(
            @Nullable TransitStation origin, T destination, Trigger trigger) {
        Trip trip = new Trip(origin, destination, trigger);
        trip.travelSyncInternal();
        return destination;
    }

    private void travelSyncInternal() {
        embark();
        if (mOrigin != null) {
            Log.i(TAG, "Trip %d: Embarked at %s towards %s", mId, mOrigin, mDestination);
        } else {
            Log.i(TAG, "Trip %d: Starting at entry point %s", mId, mDestination);
        }

        triggerTransition();
        Log.i(TAG, "Trip %d: Triggered transition, waiting to arrive at %s", mId, mDestination);

        waitUntilArrival();
        Log.i(TAG, "Trip %d: Arrived at %s", mId, mDestination);

        PublicTransitConfig.maybePauseAfterTransition(mDestination);
    }

    private void embark() {
        if (mOrigin != null) {
            mOrigin.setStateTransitioningFrom();
        }
        mDestination.setStateTransitioningTo();
    }

    private void waitUntilArrival() {
        ArrayList<ConditionWaiter.ConditionWaitStatus> waitStatuses = new ArrayList<>();

        if (mOrigin != null) {
            for (Condition condition : mOrigin.getExitConditions()) {
                waitStatuses.add(
                        new ConditionWaiter.ConditionWaitStatus(
                                condition, ConditionWaiter.ConditionOrigin.EXIT));
            }
            for (Condition condition : mOrigin.getActiveFacilityExitConditions()) {
                waitStatuses.add(
                        new ConditionWaiter.ConditionWaitStatus(
                                condition, ConditionWaiter.ConditionOrigin.EXIT));
            }
        }

        for (Condition condition : mDestination.getEnterConditions()) {
            waitStatuses.add(
                    new ConditionWaiter.ConditionWaitStatus(
                            condition, ConditionWaiter.ConditionOrigin.ENTER));
        }
        for (Condition condition : getTransitionConditions()) {
            waitStatuses.add(
                    new ConditionWaiter.ConditionWaitStatus(
                            condition, ConditionWaiter.ConditionOrigin.TRANSITION));
        }

        // Throws CriteriaNotSatisfiedException if any conditions aren't met within the timeout and
        // prints the state of all conditions. The timeout can be reduced when explicitly looking
        // for flakiness due to tight timeouts.
        try {
            ConditionWaiter.waitFor(waitStatuses);
        } catch (AssertionError e) {
            throw new TravelException(mOrigin, mDestination, e);
        }

        if (mOrigin != null) {
            mOrigin.setStateFinished();
        }
        mDestination.setStateActive();
        TrafficControl.notifyActiveStationChanged(mDestination);
    }
}
