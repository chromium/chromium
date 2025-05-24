// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import org.chromium.base.test.transit.ConditionalState.Phase;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.ArrayList;
import java.util.List;

/**
 * A {@link Transition} into a {@link Station}, either from another {@link Station} or as an entry
 * point.
 */
@NullMarked
class Trip extends Transition {
    private final List<Station<?>> mOrigins;
    private final List<Station<?>> mDestinations;

    /**
     * Constructor. Trip is instantiated to move between {@link Station}s.
     *
     * <p>The initial Transition is entering a single station.
     *
     * <p>Besides this, usually trips are triggered from one {@link Station} to another. With
     * multiwindow, there can be any number of stations being exited and any number of stations
     * being entered.
     *
     * @param origins the {@link Station}s to depart from.
     * @param destinations the {@link Station}s to travel to.
     * @param options the {@link TransitionOptions}.
     * @param trigger the action that triggers the transition. e.g. clicking a View.
     */
    Trip(
            List<Station<?>> origins,
            List<Station<?>> destinations,
            TransitionOptions options,
            @Nullable Trigger trigger) {
        super(
                options,
                getStationsPlusFacilitiesWithPhase(origins, Phase.ACTIVE),
                getStationsPlusFacilitiesWithPhase(destinations, Phase.NEW),
                trigger);
        mOrigins = origins;
        mDestinations = destinations;
    }

    private static List<? extends ConditionalState> getStationsPlusFacilitiesWithPhase(
            List<Station<?>> stations, @Phase int phase) {
        List<ConditionalState> allConditionalStates = new ArrayList<>();
        for (Station<?> station : stations) {
            allConditionalStates.add(station);
            allConditionalStates.addAll(station.getFacilitiesWithPhase(phase));
        }
        return allConditionalStates;
    }

    @Override
    protected void onAfterTransition() {
        super.onAfterTransition();
        TrafficControl.notifyActiveStationsChanged(mOrigins, mDestinations);
    }

    @Override
    public String toDebugString() {
        return String.format(
                "Trip %d (%s to %s)",
                mId, getStateListString(mOrigins), getStateListString(mDestinations));
    }
}
