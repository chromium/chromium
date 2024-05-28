// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import org.chromium.base.test.transit.ConditionWaiter.ConditionWait;
import org.chromium.base.test.transit.ConditionalState.Phase;

import java.util.List;

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
        Elements originElements =
                mOrigin.getElementsIncludingFacilitiesWithPhase(Phase.TRANSITIONING_FROM);
        Elements destinationElements =
                mDestination.getElementsIncludingFacilitiesWithPhase(Phase.TRANSITIONING_TO);

        return calculateConditionWaits(
                originElements, destinationElements, getTransitionConditions());
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
