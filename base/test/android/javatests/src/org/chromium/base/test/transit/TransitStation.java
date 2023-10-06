// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

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
 * <p>Transitions should be done with {@link Trip#goSync(TransitStation, TransitStation, Trigger)}.
 * The transit-layer derived class should expose screen-specific methods for the test-layer to use.
 */
public abstract class TransitStation extends ConditionalState {
    private static final String TAG = "Transit";
    private final int mId;
    private static int sLastStationId;
    private List<StationFacility> mFacilities = new ArrayList<>();

    protected TransitStation() {
        mId = ++sLastStationId;
    }

    List<Condition> getActiveFacilityExitConditions() {
        List<Condition> conditions = new ArrayList<>();
        for (StationFacility facility : mFacilities) {
            if (facility.getPhase() == Phase.ACTIVE) {
                conditions.addAll(facility.getExitConditions());
            }
        }
        return conditions;
    }

    void registerFacility(StationFacility facility) {
        mFacilities.add(facility);
    }

    @Override
    public String toString() {
        return String.format("<S%d: %s>", mId, getClass().getSimpleName());
    }

    /**
     * @return the self-incrementing id for logging purposes.
     */
    public int getId() {
        return mId;
    }
}
