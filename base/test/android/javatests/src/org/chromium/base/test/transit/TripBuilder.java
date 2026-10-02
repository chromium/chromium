// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import android.annotation.SuppressLint;
import android.app.Activity;

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
    private final List<State> mStatesToEnter = new ArrayList<>();
    private final List<State> mStatesToExit = new ArrayList<>();
    private final List<Condition> mConditions = new ArrayList<>();
    private final List<Trigger> mTriggers = new ArrayList<>();
    private TransitionOptions mOptions = TransitionOptions.DEFAULT;
    private @Nullable Station<?> mDestinationStation;
    private @Nullable Station<?> mOriginStation;
    private @Nullable Station<?> mContextStation;
    private @Nullable Facility<?> mContextFacility;
    private @Nullable State mContextState;
    private boolean mInNewTask;
    private boolean mHopOn;
    private @Nullable Activity mHopOnActivity;
    private boolean mIsComplete;

    public TripBuilder() {}

    /** Set the Trigger to a |runnable|. */
    @CheckReturnValue
    public TripBuilder withRunnableTrigger(Runnable runnable) {
        mTriggers.clear();
        mTriggers.add(runnable::run);
        return this;
    }

    /** Set the Trigger to a |trigger|. */
    @CheckReturnValue
    public TripBuilder withTrigger(Trigger trigger) {
        mTriggers.clear();
        mTriggers.add(trigger);
        return this;
    }

    /** Add a |trigger| to run after the already set triggers. */
    @CheckReturnValue
    public TripBuilder withAdditionalTrigger(Trigger trigger) {
        mTriggers.add(trigger);
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
        } else if (conditionalState instanceof State state) {
            mContextState = state;
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
    public TripBuilder enterStateAnd(State state) {
        state.assertInPhase(Phase.NEW);
        mStatesToEnter.add(state);
        return this;
    }

    @CheckReturnValue
    public TripBuilder exitStateAnd() {
        assert mContextState != null
                : "Context State not set, pass the state to exit as a parameter";
        return exitStateAnd(mContextState);
    }

    @CheckReturnValue
    public TripBuilder exitStateAnd(State state) {
        state.assertInPhase(Phase.ACTIVE);
        mStatesToExit.add(state);
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
            var _ = enterFacilityAnd(facility);
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
            var _ = exitFacilityAnd(facility);
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

    public <StateT extends State> StateT enterState(StateT state) {
        enterStateAnd(state).complete();
        return state;
    }

    public void exitState() {
        exitStateAnd().complete();
    }

    public void exitState(State state) {
        exitStateAnd(state).complete();
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

    /**
     * Hop on Public Transit: enter |destination| in |activity| without coming from an origin
     * Station.
     *
     * <p>This is the counterpart of {@link #arriveAt(Station, Facility[])} for code that is not
     * running inside Public Transit, most notably test utils shared with non-migrated tests. No
     * exit Conditions are checked, since there is no origin Station to exit.
     *
     * <p>Contract: hop on only when there is no live Station to transition from. A test util that
     * takes an Activity rather than a Station is, by its signature, in that position: it cannot
     * know where its caller is, so it hops on and the destination becomes the one ACTIVE Station.
     * Code that does hold a live Station should transition from it with {@link #arriveAt(Station,
     * Facility[])} instead, which verifies the exit Conditions and keeps the caller's Station
     * usable. Hopping on over an ACTIVE Station in the same Activity is logged as a warning.
     *
     * <p>The Activity is required rather than searched for: hopping on means the UI already exists,
     * so the caller always holds the Activity it is acting on, and with more than one window open,
     * matching by class either finds the wrong window or matches two and fails. The flip side is
     * that the instance must be the one that will be RESUMED: do not hop on by instance when an
     * Activity recreation (e.g. a theme change) may be in flight.
     *
     * <p>Hopping on means being outside Public Transit, so any Stations still considered active, in
     * every window, are hopped off from first; see {@link TrafficControl#hopOffPublicTransit()}.
     * This happens before the trigger runs, so if the hop-on Transition fails there is no ACTIVE
     * Station left: the next hop-on starts clean rather than inheriting a half-finished state.
     */
    @CheckReturnValue
    public TripBuilder hopOnToAnd(Activity activity, Station<?> destination) {
        assert mDestinationStation == null
                : "Destination already set to " + mDestinationStation.getName();
        assert !mInNewTask : "hopOnTo() cannot be combined with inNewTask()";
        assert mContextStation == null
                : String.format(
                        "hopOnTo() enters Public Transit, so it cannot have %s as context Station",
                        mContextStation.getName());
        destination.assertInPhase(Phase.NEW);
        mHopOn = true;
        mHopOnActivity = activity;
        mDestinationStation = destination;
        return this;
    }

    /**
     * Execute the transition synchronously, hopping on Public Transit at |destination| in
     * |activity|.
     *
     * @see #hopOnToAnd(Activity, Station)
     */
    public <T extends Station<?>> T hopOnTo(
            Activity activity, T destination, Facility<?>... facilities) {
        enterFacilitiesAnd(facilities).hopOnToAnd(activity, destination).complete();
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
        assert !mTriggers.isEmpty() : "Trigger not set";
        assert !mInNewTask || mDestinationStation != null
                : "A new Station needs to be entered in the new task";

        // Hopping on means entering Public Transit from outside of it: there is no origin Station
        // to infer, and whatever was still considered active is abandoned.
        if (mHopOn) {
            warnIfHoppingOnOverActiveStation();
            TrafficControl.hopOffPublicTransit();
        }

        // If a context Station is required, infer it from the active Stations.
        // A context Station is required to travel to a Station or to enter Facilities.
        if (!mHopOn
                && mContextStation == null
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
            if (mHopOn) {
                assert mHopOnActivity != null;
                mDestinationStation.requireActivityInstance(mHopOnActivity);
            } else if (mInNewTask) {
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
                        mStatesToExit,
                        mStatesToEnter,
                        mOptions,
                        buildCompleteTrigger());
        trip.transitionSync();

        mIsComplete = true;
        return trip;
    }

    /**
     * Hopping on is for code that is outside Public Transit. If an ACTIVE Station exists in the
     * very Activity being hopped on at, the caller most likely is a Public Transit test calling a
     * converted test util, and should transition from its Station instead. That is not an error,
     * since the util cannot know whether that Station is still accurate, but it is worth surfacing:
     * the discarded Station becomes unusable, and its exit Conditions are never checked.
     */
    private void warnIfHoppingOnOverActiveStation() {
        assert mHopOnActivity != null && mDestinationStation != null;
        for (Station<?> station : TrafficControl.getActiveStations()) {
            ActivityElement<?> activityElement = station.getActivityElement();
            if (activityElement == null || activityElement.get() != mHopOnActivity) continue;
            Log.w(
                    TAG,
                    "hopOnTo(%s) discards %s, which is ACTIVE in the same Activity. If the caller"
                            + " is a Public Transit test, transition from %s instead of calling a"
                            + " util that hops on.",
                    mDestinationStation.getName(),
                    station.getName(),
                    station.getName());
        }
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
        assert !mTriggers.isEmpty() : "Trigger not set";
        String justRunErrorMessage =
                "justRun() will not enter or leave any ConditionalStates or check any Conditions";
        assert mOriginStation == null : justRunErrorMessage;
        assert mDestinationStation == null : justRunErrorMessage;
        assert mFacilitiesToExit.isEmpty() : justRunErrorMessage;
        assert mFacilitiesToEnter.isEmpty() : justRunErrorMessage;
        assert mStatesToExit.isEmpty() : justRunErrorMessage;
        assert mStatesToEnter.isEmpty() : justRunErrorMessage;
        assert mConditions.isEmpty() : justRunErrorMessage;

        Trigger trigger = buildCompleteTrigger();
        assert trigger != null;
        try {
            if (mOptions.getRunTriggerOnUiThread()) {
                Log.i(TAG, "Will run trigger on UI thread");
                ThreadUtils.runOnUiThread(trigger::triggerTransition);
            } else {
                Log.i(TAG, "Will run trigger on Instrumentation thread");
                trigger.triggerTransition();
            }
            Log.i(TAG, "Finished running trigger");
        } catch (Throwable e) {
            throw TravelException.newTravelException(String.format("Trigger threw "), e);
        }
    }

    private @Nullable Trigger buildCompleteTrigger() {
        if (mTriggers.isEmpty()) {
            return null;
        } else if (mTriggers.size() == 1) {
            return mTriggers.get(0);
        } else {
            return () -> {
                for (Trigger trigger : mTriggers) {
                    trigger.triggerTransition();
                }
            };
        }
    }
}
