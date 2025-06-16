// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.EnsuresNonNullIf;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/** A transition into and/or out of {@link ConditionalState}s. */
@NullMarked
public abstract class Transition {
    /**
     * A trigger that will be executed to start the transition after all Conditions are in place and
     * states are set to TRANSITIONING_*.
     */
    public interface Trigger {
        /** Code to trigger the transition, e.g. click a View. */
        void triggerTransition();
    }

    // NOOP_TRIGGER is a trigger that does nothing when triggered and is special-cased in
    // Transition to be handled like a null Trigger. This causes the Transition to not fail if
    // all Conditions are already fulfilled in preCheck().
    public static final Trigger NOOP_TRIGGER = () -> {};

    private static final String TAG = "Transit";
    private static int sLastTripId;

    protected final int mId;
    protected final TransitionOptions mOptions;
    protected final @Nullable Trigger mTrigger;
    protected final List<? extends ConditionalState> mExitedStates;
    protected final List<? extends ConditionalState> mEnteredStates;
    protected final ConditionWaiter mConditionWaiter;

    Transition(
            TransitionOptions options,
            List<? extends ConditionalState> exitedStates,
            List<? extends ConditionalState> enteredStates,
            @Nullable Trigger trigger) {
        mId = ++sLastTripId;
        mOptions = options;
        mTrigger = trigger;
        mExitedStates = exitedStates;
        mEnteredStates = enteredStates;
        mConditionWaiter = new ConditionWaiter(this);
    }

    void transitionSync() {
        try {
            Log.i(TAG, "%s: started", toDebugString());
            onBeforeTransition();
            performTransitionWithRetries();
            onAfterTransition();
            Log.i(TAG, "%s: finished", toDebugString());
        } catch (TravelException e) {
            throw e;
        } catch (Throwable e) {
            throw newTransitionException(e);
        }

        PublicTransitConfig.maybePauseAfterTransition(this);
    }

    private boolean shouldFailOnAlreadyFulfilled() {
        // At least one Condition should be not fulfilled, or this is likely an incorrectly
        // designed Transition. Exceptions to this rule:
        //     1. null or NOOP_TRIGGER Trigger, for example when focusing on secondary elements of a
        //        screen that aren't declared in the Station's constructor or in its
        //        declareExtraElements().
        //     2. A explicit exception is made with TransitionOptions.mPossiblyAlreadyFulfilled.
        //        E.g. when not possible to determine whether the trigger needs to be run.
        return !mOptions.getPossiblyAlreadyFulfilled() && hasTrigger();
    }

    protected void onBeforeTransition() {
        for (ConditionalState exited : mExitedStates) {
            exited.setStateTransitioningFrom();
        }
        for (ConditionalState entered : mEnteredStates) {
            entered.setStateTransitioningTo();
        }
        mConditionWaiter.onBeforeTransition(shouldFailOnAlreadyFulfilled());
    }

    protected void onAfterTransition() {
        for (ConditionalState exited : mExitedStates) {
            exited.setStateFinished();
        }
        for (ConditionalState entered : mEnteredStates) {
            entered.setStateActive();
        }
        mConditionWaiter.onAfterTransition();
    }

    protected void performTransitionWithRetries() {
        int tries = mOptions.getTries();
        if (tries == 1) {
            triggerTransition();
            Log.i(TAG, "%s: waiting for conditions", toDebugString());
            waitUntilConditionsFulfilled();
        } else {
            for (int tryNumber = 1; tryNumber <= tries; tryNumber++) {
                try {
                    triggerTransition();
                    Log.i(
                            TAG,
                            "%s: try #%d/%d, waiting for conditions",
                            toDebugString(),
                            tryNumber,
                            tries);
                    waitUntilConditionsFulfilled();
                    break;
                } catch (TravelException e) {
                    Log.w(TAG, "%s: try #%d failed", toDebugString(), tryNumber, e);
                    if (tryNumber >= tries) {
                        throw e;
                    }
                }
            }
        }
    }

    protected void triggerTransition() {
        if (hasTrigger()) {
            try {
                if (mOptions.getRunTriggerOnUiThread()) {
                    Log.i(TAG, "%s: will run trigger on UI thread", toDebugString());
                    ThreadUtils.runOnUiThread(mTrigger::triggerTransition);
                } else {
                    Log.i(TAG, "%s: will run trigger on Instrumentation thread", toDebugString());
                    mTrigger.triggerTransition();
                }
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
            mConditionWaiter.waitFor();
        } catch (TravelException e) {
            throw e;
        } catch (Throwable e) {
            throw newTransitionException(e);
        }
    }

    protected List<Condition> getTransitionConditions() {
        if (mOptions.mTransitionConditions == null) {
            return Collections.EMPTY_LIST;
        } else {
            return mOptions.mTransitionConditions;
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

    @Override
    public String toString() {
        return toDebugString();
    }

    public List<? extends ConditionalState> getEnteredStates() {
        return mEnteredStates;
    }

    public List<? extends ConditionalState> getExitedStates() {
        return mExitedStates;
    }

    public TransitionOptions getOptions() {
        return mOptions;
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

    /** Convenience method equivalent to newOptions().withPossiblyAlreadyFulfilled().build(). */
    public static TransitionOptions possiblyAlreadyFulfilledOption() {
        return newOptions().withPossiblyAlreadyFulfilled().build();
    }

    /** Convenience method equivalent to newOptions().withCondition().withCondition().build(). */
    public static TransitionOptions conditionOption(Condition... conditions) {
        TransitionOptions.Builder builder = newOptions();
        for (Condition condition : conditions) {
            builder = builder.withCondition(condition);
        }
        return builder.build();
    }

    /** Convenience method equivalent to newOptions().withRunTriggerOnUiThread().build(). */
    public static TransitionOptions runTriggerOnUiThreadOption() {
        return newOptions().withRunTriggerOnUiThread().build();
    }

    /** Options to configure the Transition. */
    public static class TransitionOptions {

        static final TransitionOptions DEFAULT = new TransitionOptions();
        private @Nullable List<Condition> mTransitionConditions;
        private @Nullable Long mTimeoutMs;
        private @Nullable Integer mTries;
        private @Nullable Boolean mPossiblyAlreadyFulfilled;
        private @Nullable Boolean mRunTriggerOnUiThread;

        private TransitionOptions() {}

        long getTimeoutMs() {
            return mTimeoutMs != null ? mTimeoutMs : ConditionWaiter.MAX_TIME_TO_POLL;
        }

        int getTries() {
            return mTries != null ? mTries : 1;
        }

        boolean getPossiblyAlreadyFulfilled() {
            return mPossiblyAlreadyFulfilled != null && mPossiblyAlreadyFulfilled;
        }

        boolean getRunTriggerOnUiThread() {
            return mRunTriggerOnUiThread != null && mRunTriggerOnUiThread;
        }

        /**
         * Merge two TransitionOptions.
         *
         * <p>Parameters from both |primary| and |secondary| are included, with |primary| taking
         * precedence if both have the same parameter. The exceptions is Conditions: all Conditions
         * from |primary| and from |secondary| are included.
         *
         * <p>Returns a new TransitionOptions instance.
         */
        static TransitionOptions merge(TransitionOptions primary, TransitionOptions secondary) {
            TransitionOptions.Builder builder = newOptions();

            // Merge Transition Conditions
            if (secondary.mTransitionConditions != null) {
                for (Condition condition : secondary.mTransitionConditions) {
                    builder.withCondition(condition);
                }
            }
            if (primary.mTransitionConditions != null) {
                for (Condition condition : primary.mTransitionConditions) {
                    builder.withCondition(condition);
                }
            }

            if (primary.mTimeoutMs != null) {
                builder.withTimeout(primary.mTimeoutMs);
            } else if (secondary.mTimeoutMs != null) {
                builder.withTimeout(secondary.mTimeoutMs);
            }

            if (primary.mTries != null) {
                builder.withTries(primary.mTries);
            } else if (secondary.mTries != null) {
                builder.withTries(secondary.mTries);
            }

            if (primary.mPossiblyAlreadyFulfilled != null) {
                builder.withPossiblyAlreadyFulfilled(primary.mPossiblyAlreadyFulfilled);
            } else if (secondary.mPossiblyAlreadyFulfilled != null) {
                builder.withPossiblyAlreadyFulfilled(secondary.mPossiblyAlreadyFulfilled);
            }

            if (primary.mRunTriggerOnUiThread != null) {
                builder.withRunTriggerOnUiThread(primary.mRunTriggerOnUiThread);
            } else if (secondary.mRunTriggerOnUiThread != null) {
                builder.withRunTriggerOnUiThread(secondary.mRunTriggerOnUiThread);
            }

            return builder.build();
        }

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
                return withTries(2);
            }

            /**
             * Default behavior, do not retry the transition trigger if the transition does not
             * finish within the timeout.
             */
            public Builder withNoRetry() {
                return withTries(1);
            }

            private Builder withTries(int tries) {
                mTries = tries;
                return this;
            }

            /** The Transition's Conditions might already be all fulfilled before the Trigger. */
            public Builder withPossiblyAlreadyFulfilled() {
                return withPossiblyAlreadyFulfilled(true);
            }

            /**
             * Set whether to fail if the Transition's Conditions are already fulfilled before the
             * Trigger. Default behavior is to fail.
             */
            public Builder withPossiblyAlreadyFulfilled(boolean newValue) {
                mPossiblyAlreadyFulfilled = newValue;
                return this;
            }

            /** Run the {@link Trigger} on the UI Thread. */
            public Builder withRunTriggerOnUiThread() {
                return withRunTriggerOnUiThread(true);
            }

            /**
             * Set whether to run the {@link Trigger} on the UI Thread. Default behavior is to run
             * it on the instrumentation thread.
             */
            public Builder withRunTriggerOnUiThread(boolean newValue) {
                mRunTriggerOnUiThread = newValue;
                return this;
            }
        }
    }

    protected String getStateListString(List<? extends ConditionalState> states) {
        if (states.isEmpty()) {
            return "<none>";
        } else if (states.size() == 1) {
            return states.get(0).toString();
        } else {
            return states.toString();
        }
    }

    @EnsuresNonNullIf("mTrigger")
    boolean hasTrigger() {
        return mTrigger != null && mTrigger != NOOP_TRIGGER;
    }
}
