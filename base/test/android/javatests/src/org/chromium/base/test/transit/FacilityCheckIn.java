// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import androidx.annotation.Nullable;

import org.chromium.base.Log;

import java.util.ArrayList;
import java.util.List;

/** A {@link Transition} into a {@link StationFacility}. */
class FacilityCheckIn extends Transition {
    private static final String TAG = "Transit";

    private StationFacility mFacility;

    /**
     * Constructor. FacilityCheckIn is instantiated to enter a {@link StationFacility}.
     *
     * @param facility the {@link StationFacility} to enter.
     * @param trigger the action that triggers the transition into the facility. e.g. clicking a
     *     View.
     */
    FacilityCheckIn(StationFacility facility, @Nullable Trigger trigger) {
        super(trigger);
        mFacility = facility;
    }

    void enterSync() {
        onBeforeTransition();
        triggerTransition();
        List<ConditionWaiter.ConditionWaitStatus> waitStatuses = createWaitStatuses();
        waitUntilEntry(waitStatuses);
        onAfterTransition();
        PublicTransitConfig.maybePauseAfterTransition(mFacility);
    }

    private void onBeforeTransition() {
        mFacility.setStateTransitioningTo();
        Log.i(TAG, "Will enter %s", mFacility);
    }

    @Override
    protected void triggerTransition() {
        super.triggerTransition();
        Log.i(TAG, "Triggered entry into %s", mFacility);
    }

    private List<ConditionWaiter.ConditionWaitStatus> createWaitStatuses() {
        ArrayList<ConditionWaiter.ConditionWaitStatus> waitStatuses = new ArrayList<>();
        for (Condition condition : mFacility.getEnterConditions()) {
            waitStatuses.add(
                    new ConditionWaiter.ConditionWaitStatus(
                            condition, ConditionWaiter.ConditionOrigin.ENTER));
        }
        for (Condition condition : getTransitionConditions()) {
            waitStatuses.add(
                    new ConditionWaiter.ConditionWaitStatus(
                            condition, ConditionWaiter.ConditionOrigin.TRANSITION));
        }
        return waitStatuses;
    }

    private void waitUntilEntry(List<ConditionWaiter.ConditionWaitStatus> transitionConditions) {
        try {
            ConditionWaiter.waitFor(transitionConditions);
        } catch (AssertionError e) {
            throw TravelException.newEnterFacilityException(mFacility, e);
        }
    }

    private void onAfterTransition() {
        mFacility.setStateActive();
        Log.i(TAG, "Entered %s", mFacility);
    }
}
