// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import static org.junit.Assert.fail;

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
    private final List<Facility> mFacilities = new ArrayList<>();
    private final String mName;

    protected Station() {
        mId = sLastStationId++;
        TrafficControl.notifyCreatedStation(this);
        mName = String.format("<S%d: %s>", mId, getClass().getSimpleName());
    }

    Elements getElementsIncludingFacilitiesWithPhase(@Phase int phase) {
        Elements.Builder allElements = Elements.newBuilder();
        allElements.addAll(getElements());
        for (Facility facility : mFacilities) {
            if (facility.getPhase() == phase) {
                allElements.addAll(facility.getElements());
            }
        }
        return allElements.build();
    }

    void registerFacility(Facility facility) {
        mFacilities.add(facility);
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

    protected void assertSuppliersCanBeUsed() {
        int phase = getPhase();
        if (phase != Phase.ACTIVE && phase != Phase.TRANSITIONING_FROM) {
            fail(
                    String.format(
                            "%s should have been ACTIVE or TRANSITIONING_FROM, but was %s",
                            this, phaseToString(phase)));
        }
    }

    @Override
    void setStateTransitioningTo() {
        super.setStateTransitioningTo();

        for (Facility facility : mFacilities) {
            facility.setStateTransitioningTo();
        }
    }

    @Override
    void setStateActive() {
        super.setStateActive();

        for (Facility facility : mFacilities) {
            facility.setStateActive();
        }
    }

    @Override
    void setStateTransitioningFrom() {
        super.setStateTransitioningFrom();

        for (Facility facility : mFacilities) {
            if (facility.getPhase() == Phase.ACTIVE) {
                facility.setStateTransitioningFrom();
            }
        }
    }

    @Override
    void setStateFinished() {
        super.setStateFinished();

        for (Facility facility : mFacilities) {
            if (facility.getPhase() == Phase.TRANSITIONING_FROM) {
                facility.setStateFinished();
            }
        }
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
    public <F extends Facility> F enterFacilitySync(F facility, Trigger trigger) {
        return enterFacilitySync(facility, TransitionOptions.DEFAULT, trigger);
    }

    /** Version of {@link #enterFacilitySync(F, Trigger)} with extra TransitionOptions. */
    public <F extends Facility> F enterFacilitySync(
            F facility, TransitionOptions options, Trigger trigger) {
        assert facility.getHostStation() == this;
        FacilityCheckIn checkIn = new FacilityCheckIn(facility, options, trigger);
        registerFacility(facility);
        checkIn.transitionSync();
        return facility;
    }

    /**
     * Starts a transition out of the {@link Facility}, runs the transition |trigger| and blocks
     * until the facility is considered FINISHED (exit Conditions are fulfilled).
     *
     * @param facility the {@link Facility} to exit.
     * @param trigger the trigger to start the transition (e.g. clicking a view).
     * @param <F> the type of {@link Facility} exited.
     */
    public <F extends Facility> void exitFacilitySync(F facility, Trigger trigger) {
        exitFacilitySync(facility, TransitionOptions.DEFAULT, trigger);
    }

    /** Version of {@link #exitFacilitySync(F, Trigger)} with extra TransitionOptions. */
    public <F extends Facility> void exitFacilitySync(
            F facility, TransitionOptions options, Trigger trigger) {
        FacilityCheckOut checkOut = new FacilityCheckOut(facility, options, trigger);
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
    public <F extends Facility> F swapFacilitySync(
            Facility facilityToExit, F facilityToEnter, Trigger trigger) {
        return swapFacilitySync(
                facilityToExit, facilityToEnter, TransitionOptions.DEFAULT, trigger);
    }

    /** Version of {@link #swapFacilitySync(Facility, F, Trigger)} with extra TransitionOptions. */
    public <F extends Facility> F swapFacilitySync(
            Facility facilityToExit,
            F facilityToEnter,
            TransitionOptions options,
            Trigger trigger) {
        FacilitySwap swap =
                new FacilitySwap(List.of(facilityToExit), facilityToEnter, options, trigger);
        swap.transitionSync();
        return facilityToEnter;
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
    public <F extends Facility> F swapFacilitySync(
            List<Facility> facilitiesToExit, F facilityToEnter, Trigger trigger) {
        return swapFacilitySync(
                facilitiesToExit, facilityToEnter, TransitionOptions.DEFAULT, trigger);
    }

    /** Version of {@link #swapFacilitySync(List, F, Trigger)} with extra TransitionOptions. */
    public <F extends Facility> F swapFacilitySync(
            List<Facility> facilitiesToExit,
            F facilityToEnter,
            TransitionOptions options,
            Trigger trigger) {
        FacilitySwap swap = new FacilitySwap(facilitiesToExit, facilityToEnter, options, trigger);
        swap.transitionSync();
        return facilityToEnter;
    }

    /**
     * Add a Facility which will be entered together with this Station. Both will become ACTIVE in
     * the same Trip.
     */
    public void addInitialFacility(Facility facility) {
        assertInPhase(Phase.NEW);
        registerFacility(facility);
    }
}
