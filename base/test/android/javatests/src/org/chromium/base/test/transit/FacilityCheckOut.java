// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import androidx.annotation.Nullable;

import org.chromium.base.Log;

import java.util.ArrayList;
import java.util.Collections;
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
        List<ConditionWaiter.ConditionWaitStatus> waitStatuses = createWaitStatuses();
        waitUntilExit(waitStatuses);
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

    private List<ConditionWaiter.ConditionWaitStatus> createWaitStatuses() {
        ArrayList<ConditionWaiter.ConditionWaitStatus> waitStatuses = new ArrayList<>();
        for (ElementInState element : mFacility.getElements().getElementsInState()) {
            Condition exitCondition = element.getExitCondition(Collections.EMPTY_SET);
            if (exitCondition != null) {
                waitStatuses.add(
                        new ConditionWaiter.ConditionWaitStatus(
                                exitCondition, ConditionWaiter.ConditionOrigin.EXIT));
            }
        }

        for (Condition exitCondition : mFacility.getElements().getOtherExitConditions()) {
            waitStatuses.add(
                    new ConditionWaiter.ConditionWaitStatus(
                            exitCondition, ConditionWaiter.ConditionOrigin.EXIT));
        }

        for (Condition condition : getTransitionConditions()) {
            waitStatuses.add(
                    new ConditionWaiter.ConditionWaitStatus(
                            condition, ConditionWaiter.ConditionOrigin.TRANSITION));
        }
        return waitStatuses;
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
