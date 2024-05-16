// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

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
 * <p>Leaving the host {@link Station} causes this state to be left as well, and exit {@link
 * Condition}s will be waited upon for the {@link Trip} to be complete.
 *
 * <p>Transitions into and out of a Facility while the host {@link Station} is ACTIVE should be done
 * with {@link Station#enterFacilitySync(Facility, Trigger)} and {@link
 * Station#exitFacilitySync(Facility, Trigger)}.
 *
 * @param <HostStationT> the type of host {@link Station} this is scoped to.
 */
public abstract class Facility<HostStationT extends Station> extends ConditionalState {
    protected final HostStationT mHostStation;
    private final int mId;
    private static int sLastFacilityId = 1000;
    private String mName;

    /**
     * Constructor.
     *
     * <p>Instantiate a concrete subclass instead of this base class.
     *
     * <p>If the host {@link Station} is still NEW, the Enter conditions of this facility with be
     * added to the transition to the station.
     *
     * <p>If the host {@link Station} is already ACTIVE, call {@link
     * Station#enterFacilitySync(Facility, Trigger)} to enter this Facility synchronously with a
     * Transition.
     *
     * @param hostStation the host {@link Station} this {@link Facility} is scoped to.
     */
    protected Facility(HostStationT hostStation) {
        mId = ++sLastFacilityId;
        mHostStation = hostStation;
        mName =
                String.format(
                        "<S%d|F%d: %s>", mHostStation.getId(), mId, getClass().getSimpleName());
    }

    @Override
    public String getName() {
        return mName;
    }

    @Override
    public String toString() {
        return mName;
    }

    public HostStationT getHostStation() {
        return mHostStation;
    }
}
