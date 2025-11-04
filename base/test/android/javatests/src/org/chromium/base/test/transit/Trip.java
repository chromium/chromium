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
                return "LastStop";
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

    /** Returns a ConditionalState entered in this Trip. */
    public <StateT extends ConditionalState> StateT get(Class<StateT> stateClass) {
        ConditionalState candidate = null;
        if (Station.class.isAssignableFrom(stateClass)) {
            if (mDestinationStation == null) {
                throw new IllegalArgumentException("No destination Station");
            }
            if (!stateClass.isAssignableFrom(mDestinationStation.getClass())) {
                throw new IllegalArgumentException(
                        String.format(
                                "Destination %s is not a %s",
                                mDestinationStation.getName(), stateClass.getName()));
            }

            candidate = mDestinationStation;
        } else if (Facility.class.isAssignableFrom(stateClass)) {
            for (Facility<?> facility : mFacilitiesToEnter) {
                if (stateClass.isAssignableFrom(facility.getClass())) {
                    if (candidate != null) {
                        throw new IllegalArgumentException(
                                String.format(
                                        "Two or more Facilities are a %s: %s, %s",
                                        stateClass.getName(),
                                        candidate.getName(),
                                        facility.getName()));
                    }
                    candidate = facility;
                }
            }
            if (candidate == null) {
                throw new IllegalArgumentException(
                        String.format("No entered Facility is a %s", stateClass.getName()));
            }
        } else if (CarryOn.class.isAssignableFrom(stateClass)) {
            for (CarryOn carryOn : mCarryOnsToPickUp) {
                if (stateClass.isAssignableFrom(carryOn.getClass())) {
                    if (candidate != null) {
                        throw new IllegalArgumentException(
                                String.format(
                                        "Two or more CarryOns are a %s: %s, %s",
                                        stateClass.getName(),
                                        candidate.getName(),
                                        carryOn.getName()));
                    }
                    candidate = carryOn;
                }
            }
            if (candidate == null) {
                throw new IllegalArgumentException(
                        String.format("No CarryOn picked up matches %s", stateClass.getName()));
            }
        }
        StateT result = stateClass.cast(candidate);
        assert result != null;
        return result;
    }
}
