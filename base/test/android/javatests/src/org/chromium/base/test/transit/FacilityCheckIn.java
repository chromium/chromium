// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import androidx.annotation.Nullable;

import org.chromium.base.Log;
import org.chromium.base.test.transit.ConditionWaiter.ConditionWait;

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
     * @param options the {@link TransitionOptions}.
     * @param trigger the action that triggers the transition into the facility. e.g. clicking a
     *     View.
     */
    FacilityCheckIn(
            StationFacility facility, TransitionOptions options, @Nullable Trigger trigger) {
        super(options, trigger);
        mFacility = facility;
    }

    void enterSync() {
        onBeforeTransition();
        triggerTransition();
        List<ConditionWait> waits = createWaits();
        waitUntilEntry(waits);
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

    private List<ConditionWait> createWaits() {
        ArrayList<ConditionWait> waits = new ArrayList<>();

        for (ElementInState element : mFacility.getElements().getElementsInState()) {
            Condition enterCondition = element.getEnterCondition();
            if (enterCondition != null) {
                waits.add(new ConditionWait(enterCondition, ConditionWaiter.ConditionOrigin.ENTER));
            }
        }

        for (Condition enterCondition : mFacility.getElements().getOtherEnterConditions()) {
            waits.add(new ConditionWait(enterCondition, ConditionWaiter.ConditionOrigin.ENTER));
        }

        for (Condition condition : getTransitionConditions()) {
            waits.add(new ConditionWait(condition, ConditionWaiter.ConditionOrigin.TRANSITION));
        }
        return waits;
    }

    private void waitUntilEntry(List<ConditionWait> transitionConditions) {
        try {
            ConditionWaiter.waitFor(transitionConditions, mOptions);
        } catch (AssertionError e) {
            throw TravelException.newEnterFacilityException(mFacility, e);
        }
    }

    private void onAfterTransition() {
        mFacility.setStateActive();
        Log.i(TAG, "Entered %s", mFacility);
    }
}
