// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import org.chromium.base.test.transit.Transition.TransitionOptions;
import org.chromium.base.test.transit.Transition.Trigger;

/**
 * Facility is a {@link ConditionalState} scoped to a single host {@link Station} instance.
 *
 * <p>This should be used for example for popup dialogs, menus, temporary messages. A transit-layer
 * class should be derived from it and instantiated. It should expose facility-specific methods for
 * the test-layer to use.
 *
 * <p>As a {@link ConditionalState}, it has a defined lifecycle and must declare {@link Elements}
 * that determine its enter and exit {@link Condition}s.
 *
 * <p>Leaving the host {@link Station} causes this state to be left as well, and exit
 * {@link Condition}s will be waited upon for the {@link Trip} to be complete.
 *
 * <p>Transitions into and out of a Facility while the host {@link Station} is ACTIVE should be
 * done with {@link #enterSync(Facility, Trigger)} and {@link #exitSync(Facility,
 * Trigger)}.
 *
 * @param <HostStationT> the type of host {@link Station} this is scoped to.
 */
public abstract class Facility<HostStationT extends Station> extends ConditionalState {
    protected final HostStationT mHostStation;
    private final int mId;
    private static int sLastFacilityId = 1000;

    /**
     * Constructor.
     *
     * <p>Instantiate a concrete subclass instead of this base class.
     *
     * <p>If the host {@link Station} is still NEW, the Enter conditions of this facility with be
     * added to the transition to the station.
     *
     * <p>If the host {@link Station} is already ACTIVE, call {@link #enterSync(Facility, Trigger)}
     * to enter this Facility synchronously with a Transition.
     *
     * @param hostStation the host {@link Station} this {@link Facility} is scoped to.
     */
    protected Facility(HostStationT hostStation) {
        mId = ++sLastFacilityId;
        mHostStation = hostStation;
        mHostStation.registerFacility(this);
    }

    @Override
    public String toString() {
        return String.format(
                "<S%d|F%d: %s>", mHostStation.getId(), mId, getClass().getSimpleName());
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
    public static <F extends Facility> F enterSync(F facility, Trigger trigger) {
        return enterSync(facility, TransitionOptions.DEFAULT, trigger);
    }

    /** Version of #enterSync() with extra TransitionOptions. */
    public static <F extends Facility> F enterSync(
            F facility, TransitionOptions options, Trigger trigger) {
        FacilityCheckIn checkIn = new FacilityCheckIn(facility, options, trigger);
        checkIn.enterSync();
        return facility;
    }

    /**
     * Starts a transition out of the {@link Facility}, runs the transition |trigger| and blocks
     * until the facility is considered FINISHED (exit Conditions are fulfilled).
     *
     * @param facility the {@link Facility} to exit.
     * @param trigger the trigger to start the transition (e.g. clicking a view).
     * @return the {@link Facility} exited, now DONE.
     * @param <F> the type of {@link Facility} exited.
     */
    public static <F extends Facility> F exitSync(F facility, Trigger trigger) {
        return exitSync(facility, TransitionOptions.DEFAULT, trigger);
    }

    /** Version of #exitSync() with extra TransitionOptions. */
    public static <F extends Facility> F exitSync(
            F facility, TransitionOptions options, Trigger trigger) {
        FacilityCheckOut checkOut = new FacilityCheckOut(facility, options, trigger);
        checkOut.exitSync();
        return facility;
    }
}
