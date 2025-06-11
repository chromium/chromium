// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import android.annotation.SuppressLint;

import com.google.errorprone.annotations.CheckReturnValue;

import org.chromium.base.test.transit.Transition.TransitionOptions;
import org.chromium.base.test.transit.Transition.Trigger;
import org.chromium.build.annotations.Nullable;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Builder that composes a Trigger, Options and ConditionalStates to enter and exit into a
 * Transition.
 */
@SuppressLint("CheckResult")
public class TripBuilder {
    private final List<Facility<?>> mFacilitiesToEnter = new ArrayList<>();
    private final List<Facility<?>> mFacilitiesToExit = new ArrayList<>();
    private final List<Condition> mConditions = new ArrayList<>();
    private @Nullable Trigger mTrigger;
    private @Nullable TransitionOptions mOptions;
    private @Nullable Station<?> mDestinationStation;
    private @Nullable Station<?> mContextStation;
    private @Nullable Facility<?> mContextFacility;

    public TripBuilder() {}

    /** Set the Trigger to a |runnable|. */
    @CheckReturnValue
    public TripBuilder withRunnableTrigger(Runnable runnable) {
        mTrigger = runnable::run;
        return this;
    }

    /** Set the Trigger to a |trigger|. */
    @CheckReturnValue
    public TripBuilder withTrigger(Trigger trigger) {
        mTrigger = trigger;
        return this;
    }

    /**
     * Add a |conditionalState| as context.
     *
     * <p>When later any transition methods that involves Stations or Facilities is called, this
     * context is used. e.g. {@link #exitFacility()} without arguments exists the context Facility.
     *
     * <p>The context Facility is typically set by either getting a TripBuilder from a {@link
     * ViewElement} or from a {@link ConditionalState}.
     */
    @CheckReturnValue
    public TripBuilder withContext(ConditionalState conditionalState) {
        if (conditionalState instanceof Station<?> station) {
            mContextStation = station;
        } else if (conditionalState instanceof Facility<?> facility) {
            mContextFacility = facility;
            mContextStation = facility.getHostStation();
        }
        return this;
    }

    /** Add an |element|'s owner as context. */
    @CheckReturnValue
    public TripBuilder withContext(Element<?> element) {
        ConditionalState owner = element.getOwner();
        assert owner != null : String.format("Element %s is not bound", element.getId());
        return withContext(owner);
    }

    /**
     * Add options to the Transition.
     *
     * <p>Conditions in |options| are added to the existing ones. Other fields set in |options|
     * override existing ones.
     */
    @CheckReturnValue
    public TripBuilder withOptions(TransitionOptions options) {
        if (mOptions == null) {
            mOptions = options;
        } else {
            mOptions = TransitionOptions.merge(/* primary= */ options, /* secondary= */ mOptions);
        }
        return this;
    }

    /** Add a Transition |condition| that will be checked as part of the Transition. */
    @CheckReturnValue
    public TripBuilder waitForConditionsAnd(Condition... conditions) {
        mConditions.addAll(Arrays.asList(conditions));
        return this;
    }

    /** Add a |facility| to enter as part of the Transition. */
    @CheckReturnValue
    public TripBuilder enterFacilityAnd(Facility<?> facility) {
        mFacilitiesToEnter.add(facility);
        return this;
    }

    /** Add |facilities| to enter as part of the Transition. */
    @CheckReturnValue
    public TripBuilder enterFacilitiesAnd(Facility<?>... facilities) {
        for (Facility<?> facility : facilities) {
            enterFacilityAnd(facility);
        }
        return this;
    }

    /** Add the context Facility as a Facility to exit as part of the Transition. */
    @CheckReturnValue
    public TripBuilder exitFacilityAnd() {
        assert mContextFacility != null
                : "Context Facility not set, pass the Facility to exit as a parameter";
        mFacilitiesToExit.add(mContextFacility);
        return this;
    }

    /** Add |facility| to exit as part of the Transition. */
    @CheckReturnValue
    public TripBuilder exitFacilityAnd(Facility<?> facility) {
        mFacilitiesToExit.add(facility);
        return this;
    }

    /** Add |facilities| to exit as part of the Transition. */
    @CheckReturnValue
    public TripBuilder exitFacilitiesAnd(Facility<?>... facilities) {
        for (Facility<?> facility : facilities) {
            exitFacilityAnd(facility);
        }
        return this;
    }

    /** Add a |destination| Station to enter as part of the Transition. */
    @CheckReturnValue
    public TripBuilder arriveAtAnd(Station<?> destination) {
        assert mDestinationStation == null
                : "Destination already set to " + mDestinationStation.getName();
        mDestinationStation = destination;
        return this;
    }

    /** Execute the transition synchronously, waiting for the given Conditions. */
    public void waitForConditions(Condition... conditions) {
        waitForConditionsAnd(conditions).complete();
    }

    /** Execute the transition synchronously, entering |facility| and returning it. */
    public <FacilityT extends Facility<?>> FacilityT enterFacility(FacilityT facility) {
        enterFacilityAnd(facility).complete();
        return facility;
    }

    /** Execute the transition synchronously, entering |facilities|. */
    public void enterFacilities(Facility<?>... facilities) {
        enterFacilitiesAnd(facilities).complete();
    }

    /** Execute the transition synchronously, exiting the context Facility. */
    public void exitFacility() {
        exitFacilityAnd().complete();
    }

    /** Execute the transition synchronously, exiting |facility|. */
    public void exitFacility(Facility<?> facility) {
        exitFacilityAnd(facility).complete();
    }

    /** Execute the transition synchronously, exiting |facilities|. */
    public void exitFacilities(Facility<?>... facilities) {
        exitFacilitiesAnd(facilities).complete();
    }

    /**
     * Execute the transition synchronously, travelling to |destination|.
     *
     * <p>Also enter |facilities| as part of the same Transition.
     */
    public <T extends Station<?>> T arriveAt(T destination, Facility<?>... facilities) {
        enterFacilitiesAnd(facilities).arriveAtAnd(destination).complete();
        return destination;
    }

    /** Build and perform the Transition synchronously. */
    public void complete() {
        assert mTrigger != null : "Trigger not set";

        // TODO(crbug.com/406325581): Support Station#spawnSync().
        if (mContextStation == null) {
            List<Station<?>> activeStations = TrafficControl.getActiveStations();
            if (activeStations.size() == 1) {
                mContextStation = activeStations.get(0);
            } else {
                assert false
                        : "Context Station not set, cannot infer because there isn't exactly one"
                                + " active Station.";
            }
        }

        Transition trip = buildTrip();
        trip.transitionSync();
    }

    // TODO(crbug.com/406325581): Create a generic Trip class to replace these.
    private Transition buildTrip() {
        if (mDestinationStation != null) {
            for (Facility<?> facility : mFacilitiesToEnter) {
                mDestinationStation.addInitialFacility(facility);
            }

            // TODO(crbug.com/406325581): Support Station#spawnSync().
            mDestinationStation.requireToBeInSameTask(mContextStation);
            return new StationToStationTrip(
                    List.of(mContextStation), List.of(mDestinationStation), getOptions(), mTrigger);
        } else {
            if (!mFacilitiesToEnter.isEmpty()) {
                // TODO(crbug.com/406325581): Support entering Facilities from multiple Stations in
                // multi-window.
                for (Facility<?> facility : mFacilitiesToEnter) {
                    mContextStation.registerFacility(facility);
                }

                if (!mFacilitiesToExit.isEmpty()) {
                    return new FacilitySwap(
                            mFacilitiesToExit, mFacilitiesToEnter, getOptions(), mTrigger);
                } else {
                    return new FacilityCheckIn(mFacilitiesToEnter, getOptions(), mTrigger);
                }
            } else {
                if (!mFacilitiesToExit.isEmpty()) {
                    return new FacilityCheckOut(mFacilitiesToExit, getOptions(), mTrigger);
                } else {
                    // TODO(crbug.com/406325581): Support Transitions to/from CarryOns and
                    // Conditions-only.
                    throw new UnsupportedOperationException(
                            "TripBuilder only supports transitions with Stations and Facilities for"
                                    + " now.");
                }
            }
        }
    }

    private TransitionOptions getOptions() {
        TransitionOptions options = mOptions == null ? TransitionOptions.DEFAULT : mOptions;

        if (!mConditions.isEmpty()) {
            options =
                    TransitionOptions.merge(
                            Transition.conditionOption(mConditions.toArray(new Condition[0])),
                            mOptions);
        }
        return options;
    }
}
