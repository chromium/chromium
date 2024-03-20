// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import androidx.annotation.Nullable;

import org.chromium.base.Log;
import org.chromium.base.test.transit.ConditionWaiter.ConditionWaitStatus;
import org.chromium.base.test.transit.ConditionalState.Phase;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * A {@link Transition} into a {@link TransitStation}, either from another TransitStation or as an
 * entry point.
 */
public class Trip extends Transition {
    private static final String TAG = "Transit";
    private final int mId;

    @Nullable private final TransitStation mOrigin;
    private final TransitStation mDestination;

    private List<ConditionWaitStatus> mWaitStatuses;

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

    /**
     * Version of #travelSync() with extra Transition conditions.
     *
     * @param transitionConditions a list of the extra Conditions to wait for in the Transition
     */
    public static <T extends TransitStation> T travelSync(
            @Nullable TransitStation origin,
            T destination,
            List<Condition> transitionConditions,
            Trigger trigger) {
        Trip trip = new Trip(origin, destination, trigger);
        trip.addTransitionConditions(transitionConditions);
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

        mWaitStatuses =
                calculateConditionWaitStatuses(mOrigin, mDestination, getTransitionConditions());
        for (ConditionWaitStatus waitStatus : mWaitStatuses) {
            waitStatus.getCondition().onStartMonitoring();
        }
    }

    private void waitUntilArrival() {
        // Throws CriteriaNotSatisfiedException if any conditions aren't met within the timeout and
        // prints the state of all conditions. The timeout can be reduced when explicitly looking
        // for flakiness due to tight timeouts.
        try {
            ConditionWaiter.waitFor(mWaitStatuses);
        } catch (AssertionError e) {
            throw TravelException.newTripException(mOrigin, mDestination, e);
        }

        if (mOrigin != null) {
            mOrigin.setStateFinished();
        }
        mDestination.setStateActive();
        TrafficControl.notifyActiveStationChanged(mDestination);
    }

    private static ArrayList<ConditionWaitStatus> calculateConditionWaitStatuses(
            @Nullable TransitStation origin,
            TransitStation destination,
            List<Condition> transitionConditions) {
        ArrayList<ConditionWaitStatus> waitStatuses = new ArrayList<>();

        Elements originElements =
                origin != null
                        ? origin.getElementsIncludingFacilitiesWithPhase(Phase.TRANSITIONING_FROM)
                        : Elements.EMPTY;
        Elements destinationElements =
                destination.getElementsIncludingFacilitiesWithPhase(Phase.TRANSITIONING_TO);

        // Create ENTER Conditions for Views that should appear and LogicalElements that should
        // be true.
        Set<String> destinationElementIds = new HashSet<>();
        for (ElementInState element : destinationElements.getElementsInState()) {
            destinationElementIds.add(element.getId());
            @Nullable Condition enterCondition = element.getEnterCondition();
            if (enterCondition != null) {
                waitStatuses.add(
                        new ConditionWaitStatus(
                                enterCondition, ConditionWaiter.ConditionOrigin.ENTER));
            }
        }

        // Add extra ENTER Conditions.
        for (Condition enterCondition : destinationElements.getOtherEnterConditions()) {
            waitStatuses.add(
                    new ConditionWaitStatus(enterCondition, ConditionWaiter.ConditionOrigin.ENTER));
        }

        // Create EXIT Conditions for Views that should disappear and LogicalElements that should
        // be false.
        for (ElementInState element : originElements.getElementsInState()) {
            Condition exitCondition = element.getExitCondition(destinationElementIds);
            if (exitCondition != null) {
                waitStatuses.add(
                        new ConditionWaitStatus(
                                exitCondition, ConditionWaiter.ConditionOrigin.EXIT));
            }
        }

        // Add extra EXIT Conditions.
        for (Condition exitCondition : originElements.getOtherExitConditions()) {
            waitStatuses.add(
                    new ConditionWaitStatus(exitCondition, ConditionWaiter.ConditionOrigin.EXIT));
        }

        // Add transition (TRSTN) conditions
        for (Condition condition : transitionConditions) {
            waitStatuses.add(
                    new ConditionWaitStatus(condition, ConditionWaiter.ConditionOrigin.TRANSITION));
        }

        return waitStatuses;
    }
}
