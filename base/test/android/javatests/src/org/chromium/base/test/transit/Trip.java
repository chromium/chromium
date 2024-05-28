// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import androidx.annotation.Nullable;

import org.chromium.base.test.transit.ConditionWaiter.ConditionWait;
import org.chromium.base.test.transit.ConditionalState.Phase;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * A {@link Transition} into a {@link Station}, either from another {@link Station} or as an entry
 * point.
 */
class Trip extends Transition {
    private final Station mOrigin;
    private final Station mDestination;

    Trip(Station origin, Station destination, TransitionOptions options, Trigger trigger) {
        super(options, List.of(origin), List.of(destination), trigger);
        mOrigin = origin;
        mDestination = destination;
    }

    @Override
    protected List<ConditionWait> createWaits() {
        return calculateConditionWaits(mOrigin, mDestination, getTransitionConditions());
    }

    private static ArrayList<ConditionWait> calculateConditionWaits(
            Station origin, Station destination, List<Condition> transitionConditions) {
        ArrayList<ConditionWait> waits = new ArrayList<>();

        Elements originElements =
                origin.getElementsIncludingFacilitiesWithPhase(Phase.TRANSITIONING_FROM);
        Elements destinationElements =
                destination.getElementsIncludingFacilitiesWithPhase(Phase.TRANSITIONING_TO);

        // Create ENTER Conditions for Views that should appear and LogicalElements that should
        // be true.
        Set<String> destinationElementIds = new HashSet<>();
        for (ElementInState element : destinationElements.getElementsInState()) {
            destinationElementIds.add(element.getId());
            @Nullable Condition enterCondition = element.getEnterCondition();
            if (enterCondition != null) {
                waits.add(new ConditionWait(enterCondition, ConditionWaiter.ConditionOrigin.ENTER));
            }
        }

        // Add extra ENTER Conditions.
        for (Condition enterCondition : destinationElements.getOtherEnterConditions()) {
            waits.add(new ConditionWait(enterCondition, ConditionWaiter.ConditionOrigin.ENTER));
        }

        // Create EXIT Conditions for Views that should disappear and LogicalElements that should
        // be false.
        for (ElementInState element : originElements.getElementsInState()) {
            Condition exitCondition = element.getExitCondition(destinationElementIds);
            if (exitCondition != null) {
                waits.add(new ConditionWait(exitCondition, ConditionWaiter.ConditionOrigin.EXIT));
            }
        }

        // Add extra EXIT Conditions.
        for (Condition exitCondition : originElements.getOtherExitConditions()) {
            waits.add(new ConditionWait(exitCondition, ConditionWaiter.ConditionOrigin.EXIT));
        }

        // Add transition (TRSTN) conditions
        for (Condition condition : transitionConditions) {
            waits.add(new ConditionWait(condition, ConditionWaiter.ConditionOrigin.TRANSITION));
        }

        return waits;
    }

    @Override
    protected void onAfterTransition() {
        super.onAfterTransition();
        TrafficControl.notifyActiveStationChanged(mDestination);
    }

    @Override
    public String toDebugString() {
        return String.format("Trip %d (%s to %s)", mId, mOrigin, mDestination);
    }
}
