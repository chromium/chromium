// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import androidx.annotation.Nullable;

import org.chromium.base.Log;

import java.util.ArrayList;
import java.util.List;

/** A {@link Transition} out of a {@link StationFacility}. */
class FacilityCheckOut extends Transition {
    private static final String TAG = "Transit";

    private StationFacility mFacility;

    /**
     * Constructor. FacilityCheckOut is instantiated to leave a {@link StationFacility}.
     *
     * @param facility the {@link StationFacility} to leave.
     * @param trigger the action that triggers the transition out of the facility. e.g. clicking a
     *     View.
     */
    FacilityCheckOut(StationFacility facility, @Nullable Trigger trigger) {
        super(trigger);
        mFacility = facility;
    }

    void exitSync() {
        onBeforeTransition();
        triggerTransition();
        List<ConditionWaiter.ConditionWaitStatus> transitionConditions = createConditions();
        waitUntilExit(transitionConditions);
        onAfterTransition();
        PublicTransitConfig.maybePauseAfterTransition(mFacility);
    }

    private void onBeforeTransition() {
        mFacility.setStateTransitioningFrom();
        Log.i(TAG, "Will exit %s", mFacility);
    }

    @Override
    protected void triggerTransition() {
        super.triggerTransition();
        Log.i(TAG, "Triggered exit from %s", mFacility);
    }

    private List<ConditionWaiter.ConditionWaitStatus> createConditions() {
        ArrayList<ConditionWaiter.ConditionWaitStatus> transitionConditions = new ArrayList<>();
        for (Condition condition : mFacility.getExitConditions()) {
            transitionConditions.add(
                    new ConditionWaiter.ConditionWaitStatus(
                            condition, ConditionWaiter.ConditionOrigin.EXIT));
        }
        transitionConditions.addAll(createTransitionConditionStatuses());
        return transitionConditions;
    }

    private void waitUntilExit(List<ConditionWaiter.ConditionWaitStatus> transitionConditions) {
        try {
            ConditionWaiter.waitFor(transitionConditions);
        } catch (AssertionError e) {
            throw TravelException.newExitFacilityException(mFacility, e);
        }
    }

    private void onAfterTransition() {
        mFacility.setStateFinished();
        Log.i(TAG, "Exited %s", mFacility);
    }
}
