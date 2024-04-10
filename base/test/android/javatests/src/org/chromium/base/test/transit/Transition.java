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
        /** Code to trigger the transition, e.g. click a View. */
        void triggerTransition();
    }

    protected final TransitionOptions mOptions;
    @Nullable private final Trigger mTrigger;

    Transition(TransitionOptions options, @Nullable Trigger trigger) {
        mOptions = options;
        mTrigger = trigger;
    }

    protected void triggerTransition() {
        if (mTrigger != null) {
            try {
                mTrigger.triggerTransition();
            } catch (Exception e) {
                throw TravelException.newTravelException(
                        "Exception thrown by Transition trigger", e);
            }
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
        }
    }
}
