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
 * <p>Transitions should be done with {@link Trip#travelSync(Station, Station,
 * Trigger)}. The transit-layer derived class should expose screen-specific methods for the
 * test-layer to use.
 */
public abstract class Station extends ConditionalState {
    private static final String TAG = "Transit";
    private final int mId;
    private static int sLastStationId;
    private List<Facility> mFacilities = new ArrayList<>();
    private String mName;

    protected Station() {
        mId = ++sLastStationId;
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
}
