// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import android.app.Activity;

import com.google.errorprone.annotations.CanIgnoreReturnValue;
import com.google.errorprone.annotations.CheckReturnValue;

import org.chromium.base.test.transit.ConditionalState.Phase;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.function.Supplier;

/**
 * The state-changes half of a Trip: a Station to arrive at, Facilities and States to enter or exit,
 * and Conditions to wait for.
 *
 * <p>A {@link TripBuilder} is the other half: the Trigger and its options. The two are built
 * independently, e.g. a Facility offers a Trigger with {@code xxxTo()} and a Station offers an end
 * state as an Outcome, and joined with {@link TripBuilder#reach(Outcome)}:
 *
 * <pre>{@code
 * Outcome<ResultsFacility> results = Outcome.enter(new ResultsFacility()).andExit(searchBox);
 * ResultsFacility resultsFacility = searchBox.submitButtonElement.clickTo().reach(results);
 * }</pre>
 *
 * <p>The join works both ways: {@code trip.reach(outcome)} and {@code outcome.after(trip)} do the
 * same; pick the one that reads better at the call site.
 *
 * <p>Either half also works alone: {@link TripBuilder#complete()} runs a Trigger with no state
 * changes, and {@link #await()} waits for an Outcome with no Trigger.
 *
 * <p>The factory method sets the result type R: the Station or the first ConditionalState entered,
 * or nothing. Chained calls add more state changes without changing it, except {@link
 * #returning(Supplier)}.
 *
 * <p>An Outcome holds NEW ConditionalStates, so it can only be reached once. Methods that offer an
 * Outcome should return a new one per call.
 *
 * @param <R> the type returned by {@link TripBuilder#reach(Outcome)}, {@link #after} and {@link
 *     #await()}.
 */
@NullMarked
@CheckReturnValue
public class Outcome<R extends @Nullable Object> {
    private final List<Facility<?>> mFacilitiesToEnter = new ArrayList<>();
    private final List<Facility<?>> mFacilitiesToExit = new ArrayList<>();
    private final List<Facility<?>> mDefaultFacilitiesToSkip = new ArrayList<>();
    private final List<State> mStatesToEnter = new ArrayList<>();
    private final List<State> mStatesToExit = new ArrayList<>();
    private final List<Condition> mConditions = new ArrayList<>();
    private @Nullable Station<?> mDestination;
    private Supplier<R> mResult;
    private @Nullable Activity mHopOnActivity;
    private boolean mInNewTask;
    private boolean mReached;

    private Outcome(@Nullable Station<?> destination, Supplier<R> result) {
        mDestination = destination;
        mResult = result;
    }

    /** Arrive at |station|. Returns |station|. */
    public static <StationT extends Station<?>> Outcome<StationT> enter(StationT station) {
        station.assertInPhase(Phase.NEW);
        return new Outcome<>(station, () -> station);
    }

    /**
     * Hop on Public Transit at |station| in |activity|, without an origin Station. Returns
     * |station|.
     *
     * <p>Usually reached with {@link #await()}, since code that hops on is outside Public Transit
     * and has no Trigger to run.
     *
     * @see TripBuilder#hopOnToAnd(Activity, Station)
     */
    public static <StationT extends Station<?>> Outcome<StationT> hopOnAt(
            Activity activity, StationT station) {
        Outcome<StationT> outcome = enter(station);
        outcome.mHopOnActivity = activity;
        return outcome;
    }

    /**
     * Enter |facility|. Returns |facility|.
     *
     * <p>Takes a single Facility so the result is typed; enter more with {@link #and}.
     */
    public static <FacilityT extends Facility<?>> Outcome<FacilityT> enter(FacilityT facility) {
        return new Outcome<>(/* destination= */ null, () -> facility).and(facility);
    }

    /** Enter |state|. Returns |state|. */
    public static <StateT extends State> Outcome<StateT> enter(StateT state) {
        return new Outcome<>(/* destination= */ null, () -> state).and(state);
    }

    /** Exit |facilities|. Returns nothing. */
    public static Outcome<@Nullable Void> exit(Facility<?>... facilities) {
        return nothing().andExit(facilities);
    }

    /** Exit |state|. Returns nothing. */
    public static Outcome<@Nullable Void> exit(State state) {
        return nothing().andExit(state);
    }

    /** Wait for |conditions|. Returns nothing. */
    public static Outcome<@Nullable Void> waitFor(Condition... conditions) {
        return nothing().andWaitFor(conditions);
    }

    /** No state changes yet. Used by TripBuilder to accumulate the state changes of a Trip. */
    static Outcome<@Nullable Void> nothing() {
        return new Outcome<@Nullable Void>(/* destination= */ null, () -> null);
    }

    /** Also enter |facilities|. */
    public Outcome<R> and(Facility<?>... facilities) {
        for (Facility<?> facility : facilities) {
            facility.assertInPhase(Phase.NEW);
            mFacilitiesToEnter.add(facility);
        }
        return this;
    }

    /** Also enter |state|. */
    public Outcome<R> and(State state) {
        state.assertInPhase(Phase.NEW);
        mStatesToEnter.add(state);
        return this;
    }

    /** Also exit |facilities|. */
    public Outcome<R> andExit(Facility<?>... facilities) {
        mFacilitiesToExit.addAll(Arrays.asList(facilities));
        return this;
    }

    /** Also exit |state|. */
    public Outcome<R> andExit(State state) {
        mStatesToExit.add(state);
        return this;
    }

    /**
     * Also skip |facility|, a default Facility of the destination Station, when the app is expected
     * to arrive without it.
     */
    public Outcome<R> withoutDefaultFacility(Facility<?> facility) {
        mDefaultFacilitiesToSkip.add(facility);
        return this;
    }

    /** Also wait for |conditions|. */
    public Outcome<R> andWaitFor(Condition... conditions) {
        mConditions.addAll(Arrays.asList(conditions));
        return this;
    }

    /**
     * The destination Station is in a new task, e.g. a new window. The Stations already active are
     * not exited.
     */
    public Outcome<R> inNewTask() {
        assert mDestination != null : "A new Station needs to be entered in the new task";
        assert mHopOnActivity == null : "Hopping on cannot be combined with a new task";
        mInNewTask = true;
        return this;
    }

    /**
     * Return |result| instead, evaluated after the Trip, e.g. a record holding two Facilities
     * entered.
     */
    @SuppressWarnings("unchecked")
    public <T extends @Nullable Object> Outcome<T> returning(Supplier<T> result) {
        Outcome<T> self = (Outcome<T>) this;
        self.mResult = result;
        return self;
    }

    /**
     * Run |trip|'s Trigger and reach this Outcome. Same as {@code trip.reach(this)}.
     *
     * <p>Reads better when the end state is the point and the Trigger is long, e.g. a multi-line
     * lambda, or when the Trigger is passed in by a caller:
     *
     * <pre>{@code
     * return replacedBy(new SuggestionsFacility()).after(runTo(() -> {
     *     ...
     * }));
     * }</pre>
     *
     * @return the result of this Outcome.
     */
    @CanIgnoreReturnValue
    public R after(TripBuilder trip) {
        return trip.reach(this);
    }

    /**
     * Wait for this Outcome without running a Trigger.
     *
     * <p>For state changes caused by something already done, or that happen on their own, e.g. a
     * page finishing loading.
     *
     * @return the result of this Outcome.
     */
    @CanIgnoreReturnValue
    public R await() {
        return Triggers.noopTo().reach(this);
    }

    /**
     * Add |other|'s state changes to this Outcome. Called by TripBuilder#reach() on the Outcome
     * TripBuilder accumulates.
     */
    void mergeFrom(Outcome<?> other) {
        assert !other.mReached
                : "Outcome already reached; methods that offer one should return a new one";
        other.mReached = true;
        if (other.mDestination != null) {
            setDestination(other.mDestination, other.mHopOnActivity);
        }
        mFacilitiesToEnter.addAll(other.mFacilitiesToEnter);
        mStatesToEnter.addAll(other.mStatesToEnter);
        // Outcomes can be built before the states to exit are entered, so check them now.
        for (Facility<?> facility : other.mFacilitiesToExit) {
            facility.assertInPhase(Phase.ACTIVE);
            mFacilitiesToExit.add(facility);
        }
        for (State state : other.mStatesToExit) {
            state.assertInPhase(Phase.ACTIVE);
            mStatesToExit.add(state);
        }
        mConditions.addAll(other.mConditions);
        mDefaultFacilitiesToSkip.addAll(other.mDefaultFacilitiesToSkip);
        mInNewTask |= other.mInNewTask;
    }

    /** Set the Station to arrive at, or to hop on at if |hopOnActivity| is not null. */
    void setDestination(Station<?> destination, @Nullable Activity hopOnActivity) {
        assert mDestination == null : "Destination already set to " + mDestination.getName();
        assert hopOnActivity == null || !mInNewTask
                : "Hopping on cannot be combined with a new task";
        destination.assertInPhase(Phase.NEW);
        mDestination = destination;
        mHopOnActivity = hopOnActivity;
    }

    @Nullable Station<?> getDestination() {
        return mDestination;
    }

    @Nullable Activity getHopOnActivity() {
        return mHopOnActivity;
    }

    List<Facility<?>> getFacilitiesToEnter() {
        return mFacilitiesToEnter;
    }

    List<Facility<?>> getFacilitiesToExit() {
        return mFacilitiesToExit;
    }

    List<Facility<?>> getDefaultFacilitiesToSkip() {
        return mDefaultFacilitiesToSkip;
    }

    List<State> getStatesToEnter() {
        return mStatesToEnter;
    }

    List<State> getStatesToExit() {
        return mStatesToExit;
    }

    List<Condition> getConditions() {
        return mConditions;
    }

    boolean isInNewTask() {
        return mInNewTask;
    }

    /** The result, after the Trip. */
    R getResult() {
        return mResult.get();
    }
}
