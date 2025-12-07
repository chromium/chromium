// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import android.annotation.SuppressLint;

import com.google.errorprone.annotations.CheckReturnValue;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.transit.ConditionalState.Phase;
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
    private static final String TAG = "Transit";

    private final List<Facility<?>> mFacilitiesToEnter = new ArrayList<>();
    private final List<Facility<?>> mFacilitiesToExit = new ArrayList<>();
    private final List<CarryOn> mCarryOnsToPickUp = new ArrayList<>();
    private final List<CarryOn> mCarryOnsToDrop = new ArrayList<>();
    private final List<Condition> mConditions = new ArrayList<>();
    private @Nullable Trigger mTrigger;
    private TransitionOptions mOptions = TransitionOptions.DEFAULT;
    private @Nullable Station<?> mDestinationStation;
    private @Nullable Station<?> mOriginStation;
    private @Nullable Station<?> mContextStation;
    private @Nullable Facility<?> mContextFacility;
    private @Nullable CarryOn mContextCarryOn;
    private boolean mInNewTask;
    private boolean mIsComplete;

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
        } else if (conditionalState instanceof CarryOn carryOn) {
            mContextCarryOn = carryOn;
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
    private TripBuilder withOptions(TransitionOptions options) {
        mOptions = TransitionOptions.merge(/* primary= */ options, /* secondary= */ mOptions);
        return this;
    }

    /** Retry the transition trigger once, if the transition does not finish within the timeout. */
    @CheckReturnValue
    public TripBuilder withRetry() {
        return withOptions(Transition.retryOption());
    }

    /**
     * Do not retry the transition.
     *
     * <p>Default behavior, this is intended to unset {@link #withRetry()}.
     */
    @CheckReturnValue
    public TripBuilder withNoRetry() {
        return withOptions(Transition.newOptions().withNoRetry().build());
    }

    /** Set a different |timeoutMs| than the default to adjust how long to poll Conditions. */
    @CheckReturnValue
    public TripBuilder withTimeout(long timeoutMs) {
        return withOptions(Transition.timeoutOption(timeoutMs));
    }

    /**
     * Inform all Conditions might already be all fulfilled before the running the Trigger.
     *
     * <p>No-op triggers have the same behavior.
     */
    @CheckReturnValue
    public TripBuilder withPossiblyAlreadyFulfilled() {
        return withOptions(Transition.possiblyAlreadyFulfilledOption());
    }

    /** Run the trigger on the UI thread instead of on the instrumentation thread. */
    @CheckReturnValue
    public TripBuilder withRunOnUiThread() {
        return withOptions(Transition.runTriggerOnUiThreadOption());
    }

    /**
     * Expect the destination Station to be in a new task, and do not assume the currently active
     * Station will be exited..
     */
    @CheckReturnValue
    public TripBuilder inNewTask() {
        mInNewTask = true;
        return this;
    }

    /** Add a Transition |condition| that will be checked as part of the Transition. */
    @CheckReturnValue
    public TripBuilder waitForAnd(Condition... conditions) {
        mConditions.addAll(Arrays.asList(conditions));
        return this;
    }

    @CheckReturnValue
    public TripBuilder pickUpCarryOnAnd(CarryOn carryOn) {
        carryOn.assertInPhase(Phase.NEW);
        mCarryOnsToPickUp.add(carryOn);
        return this;
    }

    @CheckReturnValue
    public TripBuilder dropCarryOnAnd() {
        assert mContextCarryOn != null
                : "Context CarryOn not set, pass the not to drop as a parameter";
        return dropCarryOnAnd(mContextCarryOn);
    }

    @CheckReturnValue
    public TripBuilder dropCarryOnAnd(CarryOn carryOn) {
        carryOn.assertInPhase(Phase.ACTIVE);
        mCarryOnsToDrop.add(carryOn);
        return this;
    }

    /** Add a |facility| to enter as part of the Transition. */
    @CheckReturnValue
    public TripBuilder enterFacilityAnd(Facility<?> facility) {
        facility.assertInPhase(Phase.NEW);
        mFacilitiesToEnter.add(facility);
        return this;
    }

    /** Add |facilities| to enter as part of the Transition. */
    @CheckReturnValue
    public TripBuilder enterFacilitiesAnd(Facility<?>... facilities) {
        for (Facility<?> facility : facilities) {
            var unused = enterFacilityAnd(facility);
        }
        return this;
    }

    /** Add the context Facility as a Facility to exit as part of the Transition. */
    @CheckReturnValue
    public TripBuilder exitFacilityAnd() {
        assert mContextFacility != null
                : "Context Facility not set, pass the Facility to exit as a parameter";
        return exitFacilityAnd(mContextFacility);
    }

    /** Add |facility| to exit as part of the Transition. */
    @CheckReturnValue
    public TripBuilder exitFacilityAnd(Facility<?> facility) {
        facility.assertInPhase(Phase.ACTIVE);
        mFacilitiesToExit.add(facility);
        return this;
    }

    /** Add |facilities| to exit as part of the Transition. */
    @CheckReturnValue
    public TripBuilder exitFacilitiesAnd(Facility<?>... facilities) {
        for (Facility<?> facility : facilities) {
            var unused = exitFacilityAnd(facility);
        }
        return this;
    }

    /** Add a |destination| Station to enter as part of the Transition. */
    @CheckReturnValue
    public TripBuilder arriveAtAnd(Station<?> destination) {
        assert mDestinationStation == null
                : "Destination already set to " + mDestinationStation.getName();
        destination.assertInPhase(Phase.NEW);
        mDestinationStation = destination;
        return this;
    }

    /** Execute the transition synchronously, waiting for the given Conditions. */
    public void waitFor(Condition... conditions) {
        waitForAnd(conditions).complete();
    }

    public <CarryOnT extends CarryOn> CarryOnT pickUpCarryOn(CarryOnT carryOn) {
        pickUpCarryOnAnd(carryOn).complete();
        return carryOn;
    }

    public void dropCarryOn() {
        dropCarryOnAnd().complete();
    }

    public void dropCarryOn(CarryOn carryOn) {
        dropCarryOnAnd(carryOn).complete();
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

    /** Exit |lastStation|. */
    @CheckReturnValue
    public TripBuilder reachLastStopAnd(Station<?> lastStation) {
        assert mOriginStation == null : "Origin already set to " + mOriginStation.getName();
        assert mDestinationStation == null
                : "Last stop should not have a destination Station "
                        + mDestinationStation.getName();
        lastStation.assertInPhase(Phase.ACTIVE);
        mOriginStation = lastStation;
        return this;
    }

    /** Execute the transition synchronously, exiting the context Station and not entering any. */
    public void reachLastStop() {
        assert mContextStation != null : "Context Station not set";
        reachLastStopAnd(mContextStation).complete();
    }

    /** Build and perform the Transition synchronously. */
    public Trip complete() {
        assert !mIsComplete : "Transition already completed";
        assert mTrigger != null : "Trigger not set";
        assert !mInNewTask || mDestinationStation != null
                : "A new Station needs to be entered in the new task";

        // If a context Station is required, infer it from the active Stations.
        // A context Station is required to travel to a Station or to enter Facilities.
        if (mContextStation == null
                && (mDestinationStation != null || !mFacilitiesToEnter.isEmpty())) {
            List<Station<?>> activeStations = TrafficControl.getActiveStations();
            if (activeStations.size() == 1) {
                mContextStation = activeStations.get(0);
            } else {
                assert mInNewTask
                        : String.format(
                                "Context Station not set with withContext(), cannot infer because"
                                        + " there isn't exactly one active Station. Had %d active"
                                        + " Stations.",
                                activeStations.size());
            }
        }

        if (mDestinationStation != null) {
            if (mInNewTask) {
                mDestinationStation.requireToBeInNewTask();
            } else {
                // If entering a station and not in a new task, assume to be exiting an active
                // Station too.
                mOriginStation = mContextStation;
                mOriginStation.assertInPhase(Phase.ACTIVE);
                mDestinationStation.requireToBeInSameTask(mOriginStation);
            }
            for (Facility<?> facility : mFacilitiesToEnter) {
                mDestinationStation.registerFacility(facility);
            }
        } else {
            // TODO(crbug.com/406325581): Support entering Facilities from multiple Stations in
            // multi-window.
            for (Facility<?> facility : mFacilitiesToEnter) {
                mContextStation.registerFacility(facility);
            }
        }

        if (mOriginStation != null) {
            for (Facility<?> facility : mOriginStation.getFacilitiesWithPhase(Phase.ACTIVE)) {
                // Avoid trying to exit the same facility multiple times.
                if (!mFacilitiesToExit.contains(facility)) {
                    mFacilitiesToExit.add(facility);
                }
            }
        }

        if (!mConditions.isEmpty()) {
            mOptions =
                    TransitionOptions.merge(
                            Transition.conditionOption(mConditions.toArray(new Condition[0])),
                            mOptions);
        }

        Trip trip =
                new Trip(
                        mOriginStation,
                        mDestinationStation,
                        mFacilitiesToExit,
                        mFacilitiesToEnter,
                        mCarryOnsToDrop,
                        mCarryOnsToPickUp,
                        mOptions,
                        mTrigger);
        trip.transitionSync();

        mIsComplete = true;
        return trip;
    }

    /**
     * Build and perform the Transition synchronously.
     *
     * @return the entered ConditionalState of type |stateClass|.
     */
    public <StateT extends ConditionalState> StateT completeAndGet(Class<StateT> stateClass) {
        return complete().get(stateClass);
    }

    /**
     * Execute the trigger without waiting for any Conditions.
     *
     * @throws AssertionError if there are any Conditions to wait for already set.
     */
    public void executeTriggerWithoutTransition() {
        assert mTrigger != null : "Trigger not set";
        String justRunErrorMessage =
                "justRun() will not enter or leave any ConditionalStates or check any Conditions";
        assert mOriginStation == null : justRunErrorMessage;
        assert mDestinationStation == null : justRunErrorMessage;
        assert mFacilitiesToExit.isEmpty() : justRunErrorMessage;
        assert mFacilitiesToEnter.isEmpty() : justRunErrorMessage;
        assert mCarryOnsToDrop.isEmpty() : justRunErrorMessage;
        assert mCarryOnsToPickUp.isEmpty() : justRunErrorMessage;
        assert mConditions.isEmpty() : justRunErrorMessage;

        try {
            if (mOptions.getRunTriggerOnUiThread()) {
                Log.i(TAG, "Will run trigger on UI thread");
                ThreadUtils.runOnUiThread(mTrigger::triggerTransition);
            } else {
                Log.i(TAG, "Will run trigger on Instrumentation thread");
                mTrigger.triggerTransition();
            }
            Log.i(TAG, "Finished running trigger");
        } catch (Throwable e) {
            throw TravelException.newTravelException(String.format("Trigger threw "), e);
        }
    }
}
