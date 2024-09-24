// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import androidx.test.espresso.Espresso;

import org.chromium.base.test.transit.Transition.TransitionOptions;
import org.chromium.base.test.transit.Transition.Trigger;

import java.util.ArrayList;
import java.util.List;

/**
 * A major {@link ConditionalState}, a "screen" the app can be in. Only one can be active at a time.
 *
 * <p>A transit-layer class should be derived from it and instantiated.
 *
 * <p>As a {@link ConditionalState}, it has a defined lifecycle and must declare {@link Elements}
 * that determine its enter and exit {@link Condition}s.
 *
 * <p>Transitions should be done with {@link Trip#travelSync(Station, Station, Trigger)}. The
 * transit-layer derived class should expose screen-specific methods for the test-layer to use.
 */
public abstract class Station extends ConditionalState {
    private static final String TAG = "Transit";
    private static int sLastStationId;

    private final int mId;
    // All facilities that have ever been entered. Exited ones remain so that the history is can
    // be queried.
    private final List<Facility<?>> mFacilities = new ArrayList<>();
    private final String mName;

    protected Station() {
        mId = sLastStationId++;
        TrafficControl.notifyCreatedStation(this);
        mName = String.format("<S%d: %s>", mId, getClass().getSimpleName());
    }

    protected List<Facility<?>> getFacilitiesWithPhase(@Phase int phase) {
        List<Facility<?>> facilities = new ArrayList<>();
        for (Facility<?> facility : mFacilities) {
            if (facility.getPhase() == phase) {
                facilities.add(facility);
            }
        }
        return facilities;
    }

    private void registerFacility(Facility<?> facility) {
        facility.setHostStation(this);
        mFacilities.add(facility);
    }

    private void assertHasFacilities(List<Facility<?>> facilities) {
        for (var facility : facilities) {
            assert mFacilities.contains(facility) : this + " does not have " + facility;
        }
    }

    @Override
    public String getName() {
        return mName;
    }

    @Override
    public String toString() {
        return mName;
    }

    /**
     * @return the self-incrementing id for logging purposes.
     */
    public int getId() {
        return mId;
    }

    /**
     * Starts a transition from this origin {@link Station} to another destination {@link Station}.
     * Runs the transition |trigger|, and blocks until the destination {@link Station} is considered
     * ACTIVE (enter Conditions are fulfilled), the origin {@link Station} is considered FINISHED
     * (exit Conditions are fulfilled), and the {@link Trip}'s transition conditions are fulfilled.
     *
     * @param destination the {@link Facility} to arrive at.
     * @param trigger the trigger to start the transition (e.g. clicking a view).
     * @return the destination {@link Station}, now ACTIVE.
     * @param <T> the type of the destination {@link Station}.
     */
    public final <T extends Station> T travelToSync(T destination, Trigger trigger) {
        Trip trip = new Trip(this, destination, TransitionOptions.DEFAULT, trigger);
        trip.transitionSync();
        return destination;
    }

    /** Version of #travelToSync() with extra TransitionOptions. */
    public final <T extends Station> T travelToSync(
            T destination, TransitionOptions options, Trigger trigger) {
        Trip trip = new Trip(this, destination, options, trigger);
        trip.transitionSync();
        return destination;
    }

    /**
     * Starts a transition into the {@link Facility}, runs the transition |trigger| and blocks until
     * the facility is considered ACTIVE (enter Conditions are fulfilled).
     *
     * @param facility the {@link Facility} to enter.
     * @param trigger the trigger to start the transition (e.g. clicking a view).
     * @return the {@link Facility} entered, now ACTIVE.
     * @param <F> the type of {@link Facility} entered.
     */
    public <F extends Facility<?>> F enterFacilitySync(F facility, Trigger trigger) {
        return enterFacilitySync(facility, TransitionOptions.DEFAULT, trigger);
    }

    /** Version of {@link #enterFacilitySync(F, Trigger)} with extra TransitionOptions. */
    public <F extends Facility<?>> F enterFacilitySync(
            F facility, TransitionOptions options, Trigger trigger) {
        registerFacility(facility);
        FacilityCheckIn checkIn = new FacilityCheckIn(List.of(facility), options, trigger);
        checkIn.transitionSync();
        return facility;
    }

    /**
     * Starts a transition into multiple {@link Facility}s, runs the transition |trigger| and blocks
     * until the facilities are considered ACTIVE (enter Conditions are fulfilled).
     *
     * @param facilities the {@link Facility}s to enter.
     * @param trigger the trigger to start the transition (e.g. clicking a view).
     */
    public void enterFacilitiesSync(List<Facility<?>> facilities, Trigger trigger) {
        enterFacilitiesSync(facilities, TransitionOptions.DEFAULT, trigger);
    }

    /** Version of {@link #enterFacilitiesSync(List, Trigger)} with extra TransitionOptions. */
    public void enterFacilitiesSync(
            List<Facility<?>> facilities, TransitionOptions options, Trigger trigger) {
        for (Facility<?> f : facilities) {
            registerFacility(f);
        }
        FacilityCheckIn checkIn = new FacilityCheckIn(facilities, options, trigger);
        checkIn.transitionSync();
    }

    /**
     * Starts a transition out of the {@link Facility}, runs the transition |trigger| and blocks
     * until the facility is considered FINISHED (exit Conditions are fulfilled).
     *
     * @param facility the {@link Facility} to exit.
     * @param trigger the trigger to start the transition (e.g. clicking a view).
     */
    public void exitFacilitySync(Facility<?> facility, Trigger trigger) {
        exitFacilitySync(facility, TransitionOptions.DEFAULT, trigger);
    }

    /** Version of {@link #exitFacilitySync(Facility, Trigger)} with extra TransitionOptions. */
    public void exitFacilitySync(Facility<?> facility, TransitionOptions options, Trigger trigger) {
        exitFacilitiesSync(List.of(facility), options, trigger);
    }

    /**
     * Starts a transition out of multiple {@link Facility}s, runs the transition |trigger| and
     * blocks until the facilities are considered FINISHED (exit Conditions are fulfilled).
     *
     * @param facilities the {@link Facility}s to exit.
     * @param trigger the trigger to start the transition (e.g. clicking a view).
     */
    public void exitFacilitiesSync(List<Facility<?>> facilities, Trigger trigger) {
        exitFacilitiesSync(facilities, TransitionOptions.DEFAULT, trigger);
    }

    /** Version of {@link #exitFacilitiesSync(List, Trigger)} with extra TransitionOptions. */
    public void exitFacilitiesSync(
            List<Facility<?>> facilities, TransitionOptions options, Trigger trigger) {
        assertHasFacilities(facilities);
        FacilityCheckOut checkOut = new FacilityCheckOut(facilities, options, trigger);
        checkOut.transitionSync();
    }

    /**
     * Starts a transition out of a {@link Facility} and into another {@link Facility}.
     *
     * <p>Runs the transition |trigger| and blocks until |facilityToExit| is considered FINISHED
     * (exit Conditions are fulfilled) and |facilityToEnter| is considered ACTIVE (enter Conditions
     * are fulfilled).
     *
     * @param facilityToExit the {@link Facility} to exit.
     * @param facilityToEnter the {@link Facility} to enter.
     * @param trigger the trigger to start the transition (e.g. clicking a view).
     * @return the {@link Facility} entered, now ACTIVE.
     * @param <F> the type of {@link Facility} entered.
     */
    public <F extends Facility<?>> F swapFacilitySync(
            Facility<?> facilityToExit, F facilityToEnter, Trigger trigger) {
        return swapFacilitySync(
                List.of(facilityToExit), facilityToEnter, TransitionOptions.DEFAULT, trigger);
    }

    /** Version of {@link #swapFacilitySync(Facility, F, Trigger)} with extra TransitionOptions. */
    public <F extends Facility<?>> F swapFacilitySync(
            Facility<?> facilityToExit,
            F facilityToEnter,
            TransitionOptions options,
            Trigger trigger) {
        return swapFacilitySync(List.of(facilityToExit), facilityToEnter, options, trigger);
    }

    /**
     * Starts a transition out of 1+ {@link Facility}s and into another {@link Facility}.
     *
     * <p>Runs the transition |trigger| and blocks until all |facilitiesToExit| are considered
     * FINISHED (exit Conditions are fulfilled) and |facilityToEnter| is considered ACTIVE (enter
     * Conditions are fulfilled).
     *
     * @param facilitiesToExit the {@link Facility}s to exit.
     * @param facilityToEnter the {@link Facility} to enter.
     * @param trigger the trigger to start the transition (e.g. clicking a view).
     * @return the {@link Facility} entered, now ACTIVE.
     * @param <F> the type of {@link Facility} entered.
     */
    public <F extends Facility<?>> F swapFacilitySync(
            List<Facility<?>> facilitiesToExit, F facilityToEnter, Trigger trigger) {
        return swapFacilitySync(
                facilitiesToExit, facilityToEnter, TransitionOptions.DEFAULT, trigger);
    }

    /** Version of {@link #swapFacilitySync(List, F, Trigger)} with extra TransitionOptions. */
    public <F extends Facility<?>> F swapFacilitySync(
            List<Facility<?>> facilitiesToExit,
            F facilityToEnter,
            TransitionOptions options,
            Trigger trigger) {
        assertHasFacilities(facilitiesToExit);
        registerFacility(facilityToEnter);
        FacilitySwap swap = new FacilitySwap(facilitiesToExit, facilityToEnter, options, trigger);
        swap.transitionSync();
        return facilityToEnter;
    }

    /**
     * Add a Facility which will be entered together with this Station. Both will become ACTIVE in
     * the same Trip.
     */
    public <F extends Facility<?>> F addInitialFacility(F facility) {
        assertInPhase(Phase.NEW);
        registerFacility(facility);
        return facility;
    }

    /**
     * Press back expecting to get to the given destination.
     *
     * <p>Left vague because back behavior is too case-by-case to determine in the Transit Layer.
     */
    public <T extends Station> T pressBack(T destination) {
        return travelToSync(destination, Espresso::pressBack);
    }
}
