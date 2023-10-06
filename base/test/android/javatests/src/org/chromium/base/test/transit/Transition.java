// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import androidx.annotation.Nullable;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/** A transition into and/or out of {@link ConditionalState}s. */
public class Transition {
    /**
     * A trigger that will be executed to start the transition after all Conditions are in place and
     * states are set to TRANSITIONING_*.
     */
    public interface Trigger {
        /**
         * Code to trigger the transition, e.g. click a View.
         *
         * @param transition the Transition that will be triggered; Conditions can be added to it.
         */
        void triggerTransition(Transition transition);
    }

    @Nullable private final Trigger mTrigger;

    @Nullable private List<Condition> mConditions;

    Transition(@Nullable Trigger trigger) {
        mTrigger = trigger;
    }

    /**
     * Add a |condition| to the Transition that is not in the exit or enter conditions of the states
     * involved. The condition will be waited in parallel with the exit and enter conditions of the
     * states.
     */
    public void addCondition(Condition condition) {
        if (mConditions == null) {
            mConditions = new ArrayList<>();
        }
        mConditions.add(condition);
    }

    protected void triggerTransition() {
        if (mTrigger != null) {
            mTrigger.triggerTransition(this);
        }
    }

    protected List<ConditionWaiter.ConditionWaitStatus> createTransitionConditionStatuses() {
        if (mConditions == null) {
            return Collections.EMPTY_LIST;
        }

        ArrayList<ConditionWaiter.ConditionWaitStatus> statuses = new ArrayList<>();
        for (Condition condition : mConditions) {
            statuses.add(
                    new ConditionWaiter.ConditionWaitStatus(
                            condition, ConditionWaiter.ConditionOrigin.TRANSITION));
        }
        return statuses;
    }
}
