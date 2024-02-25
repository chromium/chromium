// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import org.chromium.base.test.transit.Transition.Trigger;

/**
 * StationFacility is a {@link ConditionalState} scoped to a single {@link TransitStation} instance.
 *
 * <p>This should be used for example for popup dialogs, menus, temporary messages. A transit-layer
 * class should be derived from it and instantiated. It should expose facility-specific methods for
 * the test-layer to use.
 *
 * <p>As a {@link ConditionalState}, it has a defined lifecycle and must declare {@link Elements}
 * that determine its enter and exit {@link Condition}s.
 *
 * <p>Leaving the TransitStation causes this state to be left as well, and exit Conditions will be
 * waited upon for the TransitStation transition to be complete.
 *
 * <p>Transitions into and out of a StationFacility while the TransitStation is ACTIVE should be
 * done with {@link #enterSync(StationFacility, Trigger)} and {@link #exitSync(StationFacility,
 * Trigger)}.
 *
 * @param <T> the type of TransitStation this is scoped to.
 */
public abstract class StationFacility<T extends TransitStation> extends ConditionalState {
    protected final T mStation;
    private final int mId;
    private static int sLastFacilityId = 1000;

    /**
     * Constructor.
     *
     * <p>Instantiate a subclass, then call {@link #enterSync(StationFacility, Trigger)} to enter
     * it.
     *
     * @param station the TransitStation this StationFacility is scoped to.
     */
    protected StationFacility(T station) {
        mId = ++sLastFacilityId;
        mStation = station;
        mStation.registerFacility(this);
    }

    @Override
    public String toString() {
        return String.format("<S%d|F%d: %s>", mStation.getId(), mId, getClass().getSimpleName());
    }

    /**
     * Starts a transition into the StationFacility, runs the transition |trigger| and blocks until
     * the facility is considered ACTIVE (enter Conditions are fulfilled).
     *
     * @param facility the StationFacility to enter.
     * @param trigger the trigger to start the transition (e.g. clicking a view).
     * @return the StationFacility entered.
     * @param <F> the type of StationFacility entered.
     */
    public static <F extends StationFacility> F enterSync(F facility, Trigger trigger) {
        FacilityCheckIn checkIn = new FacilityCheckIn(facility, trigger);
        checkIn.enterSync();
        return facility;
    }

    /**
     * Starts a transition out of the StationFacility, runs the transition |trigger| and blocks
     * until the facility is considered FINISHED (exit Conditions are fulfilled).
     *
     * @param facility the StationFacility to exit.
     * @param trigger the trigger to start the transition (e.g. clicking a view).
     * @return the StationFacility exited.
     * @param <F> the type of StationFacility exited.
     */
    public static <F extends StationFacility> F exitSync(F facility, Trigger trigger) {
        FacilityCheckOut checkOut = new FacilityCheckOut(facility, trigger);
        checkOut.exitSync();
        return facility;
    }
}
