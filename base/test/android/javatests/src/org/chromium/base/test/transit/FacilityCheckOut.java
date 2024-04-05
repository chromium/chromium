// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import androidx.annotation.Nullable;

import org.chromium.base.Log;
import org.chromium.base.test.transit.ConditionWaiter.ConditionWait;

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
     * @param options the {@link TransitionOptions}.
     * @param trigger the action that triggers the transition out of the facility. e.g. clicking a
     *     View.
     */
    FacilityCheckOut(
            StationFacility facility, TransitionOptions options, @Nullable Trigger trigger) {
        super(options, trigger);
        mFacility = facility;
    }

    void exitSync() {
        onBeforeTransition();
        triggerTransition();
        List<ConditionWait> waits = createWaits();
        waitUntilExit(waits);
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

    private List<ConditionWait> createWaits() {
        ArrayList<ConditionWait> waits = new ArrayList<>();
        for (ElementInState element : mFacility.getElements().getElementsInState()) {
            Condition exitCondition = element.getExitCondition(Collections.EMPTY_SET);
            if (exitCondition != null) {
                waits.add(new ConditionWait(exitCondition, ConditionWaiter.ConditionOrigin.EXIT));
            }
        }

        for (Condition exitCondition : mFacility.getElements().getOtherExitConditions()) {
            waits.add(new ConditionWait(exitCondition, ConditionWaiter.ConditionOrigin.EXIT));
        }

        for (Condition condition : getTransitionConditions()) {
            waits.add(new ConditionWait(condition, ConditionWaiter.ConditionOrigin.TRANSITION));
        }
        return waits;
    }

    private void waitUntilExit(List<ConditionWait> transitionConditions) {
        try {
            ConditionWaiter.waitFor(transitionConditions, mOptions);
        } catch (AssertionError e) {
            throw TravelException.newExitFacilityException(mFacility, e);
        }
    }

    private void onAfterTransition() {
        mFacility.setStateFinished();
        Log.i(TAG, "Exited %s", mFacility);
    }
}
