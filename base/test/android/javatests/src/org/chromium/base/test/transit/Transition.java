// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import androidx.annotation.Nullable;

import org.chromium.base.Log;
import org.chromium.base.test.transit.ConditionWaiter.ConditionWait;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/** A transition into and/or out of {@link ConditionalState}s. */
public abstract class Transition {
    /**
     * A trigger that will be executed to start the transition after all Conditions are in place and
     * states are set to TRANSITIONING_*.
     */
    public interface Trigger {
        /** Code to trigger the transition, e.g. click a View. */
        void triggerTransition();
    }

    private static final String TAG = "Transit";
    private static int sLastTripId;

    protected final int mId;
    protected final TransitionOptions mOptions;
    protected final List<? extends ConditionalState> mExitedStates;
    protected final List<? extends ConditionalState> mEnteredStates;
    @Nullable protected final Trigger mTrigger;

    protected List<ConditionWait> mWaits;

    Transition(
            TransitionOptions options,
            List<? extends ConditionalState> exitedStates,
            List<? extends ConditionalState> enteredStates,
            @Nullable Trigger trigger) {
        mId = ++sLastTripId;
        mOptions = options;
        mExitedStates = exitedStates;
        mEnteredStates = enteredStates;
        mTrigger = trigger;
    }

    void transitionSync() {
        Log.i(TAG, "%s: started", toDebugString());
        onBeforeTransition();
        performTransitionWithRetries();
        onAfterTransition();
        Log.i(TAG, "%s: finished", toDebugString());

        PublicTransitConfig.maybePauseAfterTransition(this);
    }

    private boolean shouldFailOnAlreadyFulfilled() {
        // At least one Condition should be not fulfilled, or this is likely an incorrectly
        // designed Transition. Exceptions to this rule:
        //     1. null Trigger, for example when focusing on secondary elements of a screen that
        //        aren't declared in Station#declareElements().
        //     2. A explicit exception is made with TransitionOptions.mPossiblyAlreadyFulfilled.
        //        E.g. when not possible to determine whether the trigger needs to be run.
        return !mOptions.mPossiblyAlreadyFulfilled && mTrigger != null;
    }

    protected void onBeforeTransition() {
        for (ConditionalState exited : mExitedStates) {
            exited.setStateTransitioningFrom();
        }
        for (ConditionalState entered : mEnteredStates) {
            entered.setStateTransitioningTo();
        }

        mWaits = createWaits();
        try {
            ConditionWaiter.preCheck(mWaits, shouldFailOnAlreadyFulfilled());
        } catch (Throwable e) {
            throw newTransitionException(e);
        }
        for (ConditionWait wait : mWaits) {
            wait.getCondition().onStartMonitoring();
        }
    }

    protected void performTransitionWithRetries() {
        if (mOptions.mTries == 1) {
            triggerTransition();
            Log.i(TAG, "%s: waiting for conditions", toDebugString());
            waitUntilConditionsFulfilled();
        } else {
            for (int tryNumber = 1; tryNumber <= mOptions.mTries; tryNumber++) {
                try {
                    triggerTransition();
                    Log.i(
                            TAG,
                            "%s: try #%d/%d, waiting for conditions",
                            toDebugString(),
                            tryNumber,
                            mOptions.mTries);
                    waitUntilConditionsFulfilled();
                    break;
                } catch (TravelException e) {
                    Log.w(TAG, "%s: try #%d failed", toDebugString(), tryNumber, e);
                    if (tryNumber >= mOptions.mTries) {
                        throw e;
                    }
                }
            }
        }
    }

    protected void triggerTransition() {
        if (mTrigger != null) {
            Log.i(TAG, "%s: will run trigger", toDebugString());
            try {
                mTrigger.triggerTransition();
                Log.i(TAG, "%s: finished running trigger", toDebugString());
            } catch (Throwable e) {
                throw TravelException.newTravelException(
                        String.format("%s: trigger threw ", toDebugString()), e);
            }
        } else {
            Log.i(TAG, "%s is triggerless", toDebugString());
        }
    }

    protected void waitUntilConditionsFulfilled() {
        // Throws CriteriaNotSatisfiedException if any conditions aren't met within the timeout and
        // prints the state of all conditions. The timeout can be reduced when explicitly looking
        // for flakiness due to tight timeouts.
        try {
            ConditionWaiter.waitFor(mWaits, toDebugString(), mOptions);
        } catch (Throwable e) {
            throw newTransitionException(e);
        }
    }

    protected void onAfterTransition() {
        for (ConditionalState exited : mExitedStates) {
            exited.setStateFinished();
        }
        for (ConditionalState entered : mEnteredStates) {
            entered.setStateActive();
        }

        for (ConditionWait wait : mWaits) {
            wait.getCondition().onStopMonitoring();
        }
    }

    /**
     * Factory method for TravelException for an error during a {@link Transition}.
     *
     * @param cause the root cause
     * @return a new TravelException instance
     */
    public TravelException newTransitionException(Throwable cause) {
        return TravelException.newTravelException("Did not complete " + toDebugString(), cause);
    }

    /** Should return a String representation of the Transition for debugging. */
    public abstract String toDebugString();

    /** Should create the {@link ConditionWait}s. */
    protected abstract List<ConditionWait> createWaits();

    @Override
    public String toString() {
        return toDebugString();
    }

    protected List<Condition> getTransitionConditions() {
        if (mOptions.mTransitionConditions == null) {
            return Collections.EMPTY_LIST;
        } else {
            return mOptions.mTransitionConditions;
        }
    }

    protected static ArrayList<ConditionWait> calculateConditionWaits(
            Elements originElements,
            Elements destinationElements,
            List<Condition> transitionConditions) {
        ArrayList<ConditionWait> waits = new ArrayList<>();

        // Create ENTER Conditions for Views that should appear and LogicalElements that should
        // be true.
        Set<String> destinationElementIds = new HashSet<>();
        for (ElementInState element : destinationElements.getElementsInState()) {
            destinationElementIds.add(element.getId());
            @Nullable Condition enterCondition = element.getEnterCondition();
            if (enterCondition != null) {
                waits.add(new ConditionWait(enterCondition, ConditionWaiter.ConditionOrigin.ENTER));
            }
        }

        // Add extra ENTER Conditions.
        for (Condition enterCondition : destinationElements.getOtherEnterConditions()) {
            waits.add(new ConditionWait(enterCondition, ConditionWaiter.ConditionOrigin.ENTER));
        }

        // Create EXIT Conditions for Views that should disappear and LogicalElements that should
        // be false.
        for (ElementInState element : originElements.getElementsInState()) {
            Condition exitCondition = element.getExitCondition(destinationElementIds);
            if (exitCondition != null) {
                waits.add(new ConditionWait(exitCondition, ConditionWaiter.ConditionOrigin.EXIT));
            }
        }

        // Add extra EXIT Conditions.
        for (Condition exitCondition : originElements.getOtherExitConditions()) {
            waits.add(new ConditionWait(exitCondition, ConditionWaiter.ConditionOrigin.EXIT));
        }

        // Add transition (TRSTN) conditions
        for (Condition condition : transitionConditions) {
            waits.add(new ConditionWait(condition, ConditionWaiter.ConditionOrigin.TRANSITION));
        }

        return waits;
    }

    /**
     * @return builder to specify Transition options.
     *     <p>e.g.: Transition.newOptions().withTimeout(10000L).build()
     */
    public static TransitionOptions.Builder newOptions() {
        return new TransitionOptions().new Builder();
    }

    /** Convenience method equivalent to newOptions().withTimeout().build(). */
    public static TransitionOptions timeoutOption(long timeoutMs) {
        return newOptions().withTimeout(timeoutMs).build();
    }

    /** Convenience method equivalent to newOptions().withRetry().build(). */
    public static TransitionOptions retryOption() {
        return newOptions().withRetry().build();
    }

    /** Options to configure the Transition. */
    public static class TransitionOptions {

        static final TransitionOptions DEFAULT = new TransitionOptions();
        @Nullable List<Condition> mTransitionConditions;
        long mTimeoutMs;
        int mTries = 1;
        boolean mPossiblyAlreadyFulfilled;

        private TransitionOptions() {}

        /** Builder for TransitionOptions. Call {@link Transition#newOptions()} to instantiate. */
        public class Builder {
            public TransitionOptions build() {
                return TransitionOptions.this;
            }

            /**
             * Add an extra |condition| to the Transition that is not in the exit or enter
             * conditions of the states involved.
             */
            public Builder withCondition(Condition condition) {
                if (mTransitionConditions == null) {
                    mTransitionConditions = new ArrayList<>();
                }
                mTransitionConditions.add(condition);
                return this;
            }

            /**
             * @param timeoutMs how long to poll for during the transition
             */
            public Builder withTimeout(long timeoutMs) {
                mTimeoutMs = timeoutMs;
                return this;
            }

            /**
             * Retry the transition trigger once, if the transition does not finish within the
             * timeout.
             */
            public Builder withRetry() {
                mTries = 2;
                return this;
            }

            /** The Transition's Conditions might already be all fulfilled before the Trigger. */
            public Builder withPossiblyAlreadyFulfilled() {
                mPossiblyAlreadyFulfilled = true;
                return this;
            }
        }
    }
}
