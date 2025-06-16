// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * A generic {@link Transition} out of one or more {@link Facility}s into another {@link Facility}.
 */
@NullMarked
public class Trip extends Transition {
    private final @Nullable Station<?> mOriginStation;
    private final @Nullable Station<?> mDestinationStation;
    private final List<Facility<?>> mFacilitiesToExit;
    private final List<Facility<?>> mFacilitiesToEnter;
    private final List<CarryOn> mCarryOnsToDrop;
    private final List<CarryOn> mCarryOnsToPickUp;
    private final String mTransitionName;

    /** Use {@link TripBuilder} to create a Trip. */
    Trip(
            @Nullable Station<?> originStation,
            @Nullable Station<?> destinationStation,
            List<Facility<?>> facilitiesToExit,
            List<Facility<?>> facilitiesToEnter,
            List<CarryOn> carryOnsToDrop,
            List<CarryOn> carryOnsToPickUp,
            TransitionOptions options,
            @Nullable Trigger trigger) {
        super(
                options,
                aggregateStates(originStation, facilitiesToExit, carryOnsToDrop),
                aggregateStates(destinationStation, facilitiesToEnter, carryOnsToPickUp),
                trigger);
        mOriginStation = originStation;
        mDestinationStation = destinationStation;
        mFacilitiesToExit = facilitiesToExit;
        mFacilitiesToEnter = facilitiesToEnter;
        mCarryOnsToDrop = carryOnsToDrop;
        mCarryOnsToPickUp = carryOnsToPickUp;

        String transitionTypeName = determineTransitionTypeName();
        mTransitionName = determineTransitionName(transitionTypeName);
    }

    @Override
    protected void onAfterTransition() {
        super.onAfterTransition();

        if (mOriginStation != null || mDestinationStation != null) {
            TrafficControl.notifyActiveStationsChanged(
                    mOriginStation != null ? List.of(mOriginStation) : Collections.emptyList(),
                    mDestinationStation != null
                            ? List.of(mDestinationStation)
                            : Collections.emptyList());
        }
    }

    @Override
    public String toDebugString() {
        return mTransitionName;
    }

    private static List<? extends ConditionalState> aggregateStates(
            @Nullable Station<?> station, List<Facility<?>> facilities, List<CarryOn> carryOns) {
        List<ConditionalState> states = new ArrayList<>();
        if (station != null) {
            states.add(station);
        }
        states.addAll(facilities);
        states.addAll(carryOns);
        return states;
    }

    private String determineTransitionTypeName() {
        if (mDestinationStation != null) {
            if (mOriginStation != null) {
                return "StationToStationTrip";
            } else {
                return "StationSpawn";
            }
        } else {
            if (mOriginStation != null) {
                return "LastStation";
            }
        }

        if (!mFacilitiesToEnter.isEmpty()) {
            if (!mFacilitiesToExit.isEmpty()) {
                return "FacilitySwap";
            } else {
                return "FacilityCheckIn";
            }
        } else if (!mFacilitiesToExit.isEmpty()) {
            return "FacilityCheckOut";
        }

        if (!mCarryOnsToPickUp.isEmpty()) {
            if (!mCarryOnsToDrop.isEmpty()) {
                return "CarryOnSwap";
            } else {
                return "CarryOnPickUp";
            }
        } else if (!mCarryOnsToDrop.isEmpty()) {
            return "CarryOnDrop";
        }

        return "GenericTrip";
    }

    private String determineTransitionName(String transitionTypeName) {
        if (!mEnteredStates.isEmpty()) {
            String facilitiesToEnterString = getStateListString(mEnteredStates);
            if (!mExitedStates.isEmpty()) {
                String facilitiesToExitString = getStateListString(mExitedStates);
                return String.format(
                        "%s %d (from %s to %s)",
                        transitionTypeName, mId, facilitiesToExitString, facilitiesToEnterString);
            } else {
                return String.format(
                        "%s %d (enter %s)", transitionTypeName, mId, facilitiesToEnterString);
            }
        } else {
            if (!mExitedStates.isEmpty()) {
                String facilitiesToExitString = getStateListString(mExitedStates);
                return String.format(
                        "%s %d (exit %s)", transitionTypeName, mId, facilitiesToExitString);
            } else {
                return String.format("%s %d", transitionTypeName, mId);
            }
        }
    }
}
