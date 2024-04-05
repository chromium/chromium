// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import androidx.annotation.Nullable;

import java.util.Set;
import java.util.concurrent.Callable;

/**
 * Represents a logical expression that has to be true to consider the Station active.
 *
 * <p>LogicalElements should be declared by calling {@link
 * Elements.Builder#declareLogicalElement(LogicalElement)} passing in an instance created by one of
 * the factory methods here such as {@link #sharedUiThreadLogicalElement(String, Callable)}.
 *
 * <p>Generates ENTER and EXIT Conditions for the ConditionalState to ensure the LogicalElement is
 * in the right state.
 */
public class LogicalElement implements ElementInState {

    private final boolean mIsRunOnUiThread;
    private final boolean mIsScoped;
    private final String mDescription;
    private final String mId;
    private final Condition mEnterCondition;
    @Nullable private Condition mExitCondition;

    /**
     * Create a shared-scope LogicalElement that runs the check on the UI Thread.
     *
     * <p>Unscoped LogicalElements wait for the function to be true as an ENTER Condition. It also
     * waits for the function to be false as an EXIT Condition when transitioning to a
     * ConditionalState that does not declare the LogicalElement too.
     */
    public static LogicalElement sharedUiThreadLogicalElement(
            String description, Callable<Boolean> checkFunction, String id) {
        return new LogicalElement(
                /* isRunOnUiThread= */ true, /* isScoped= */ true, description, checkFunction, id);
    }

    /**
     * Version of {@link #sharedUiThreadLogicalElement(String, Callable, String)} using the
     * |description| as |id|.
     */
    public static LogicalElement sharedUiThreadLogicalElement(
            String description, Callable<Boolean> checkFunction) {
        return new LogicalElement(
                /* isRunOnUiThread= */ true,
                /* isScoped= */ true,
                description,
                checkFunction,
                /* id= */ null);
    }

    /**
     * Create a shared-scope LogicalElement that runs the check on the Instrumentation Thread.
     *
     * <p>Unscoped LogicalElements wait for the function to be true as an ENTER Condition. It also
     * waits for the function to be false as an EXIT Condition when transitioning to a
     * ConditionalState that does not declare the LogicalElement too.
     */
    public static LogicalElement sharedInstrumentationThreadLogicalElement(
            String description, Callable<Boolean> checkFunction, String id) {
        return new LogicalElement(
                /* isRunOnUiThread= */ false, /* isScoped= */ true, description, checkFunction, id);
    }

    /**
     * Version of {@link #sharedInstrumentationThreadLogicalElement(String, Callable, String)} using
     * the |description| as |id|.
     */
    public static LogicalElement sharedInstrumentationThreadLogicalElement(
            String description, Callable<Boolean> checkFunction) {
        return new LogicalElement(
                /* isRunOnUiThread= */ false,
                /* isScoped= */ true,
                description,
                checkFunction,
                /* id= */ null);
    }

    /**
     * Create an unscoped LogicalElement that runs the check on the UI Thread.
     *
     * <p>Unscoped LogicalElements wait for the function to be true as an ENTER Condition but do not
     * generate an EXIT Condition.
     */
    public static LogicalElement unscopedUiThreadLogicalElement(
            String description, Callable<Boolean> checkFunction, String id) {
        return new LogicalElement(
                /* isRunOnUiThread= */ true, /* isScoped= */ false, description, checkFunction, id);
    }

    /**
     * Version of {@link #unscopedUiThreadLogicalElement(String, Callable, String)} using the
     * |description| as |id|.
     */
    public static LogicalElement unscopedUiThreadLogicalElement(
            String description, Callable<Boolean> checkFunction) {
        return new LogicalElement(
                /* isRunOnUiThread= */ true,
                /* isScoped= */ false,
                description,
                checkFunction,
                /* id= */ null);
    }

    /**
     * Create an unscoped LogicalElement that runs the check on the Instrumentation Thread.
     *
     * <p>Unscoped LogicalElements wait for the function to be true as an ENTER Condition but do not
     * generate an EXIT Condition.
     */
    public static LogicalElement unscopedInstrumentationThreadLogicalElement(
            String description, Callable<Boolean> checkFunction, String id) {
        return new LogicalElement(
                /* isRunOnUiThread= */ false,
                /* isScoped= */ false,
                description,
                checkFunction,
                id);
    }

    /**
     * Version of {@link #unscopedInstrumentationThreadLogicalElement(String, Callable, String)}
     * using the |description| as |id|.
     */
    public static LogicalElement unscopedInstrumentationThreadLogicalElement(
            String description, Callable<Boolean> checkFunction) {
        return new LogicalElement(
                /* isRunOnUiThread= */ false,
                /* isScoped= */ false,
                description,
                checkFunction,
                /* id= */ null);
    }

    LogicalElement(
            boolean isRunOnUiThread,
            boolean isScoped,
            String description,
            Callable<Boolean> checkFunction,
            @Nullable String id) {
        mIsRunOnUiThread = isRunOnUiThread;
        mIsScoped = isScoped;
        mDescription = description;
        mId = "LE/" + (id != null ? id : description);

        mEnterCondition =
                new Condition(mIsRunOnUiThread) {
                    @Override
                    public ConditionStatus check() throws Exception {
                        return whether(checkFunction.call());
                    }

                    @Override
                    public String buildDescription() {
                        return "True: " + mDescription;
                    }
                };

        if (mIsScoped) {
            mExitCondition =
                    new Condition(mIsRunOnUiThread) {
                        @Override
                        public ConditionStatus check() throws Exception {
                            return whether(!checkFunction.call());
                        }

                        @Override
                        public String buildDescription() {
                            return "False: " + mDescription;
                        }
                    };
        } else {
            mExitCondition = null;
        }
    }

    @Override
    public String getId() {
        return mId;
    }

    @Override
    public Condition getEnterCondition() {
        return mEnterCondition;
    }

    @Override
    public @Nullable Condition getExitCondition(Set<String> destinationElementIds) {
        if (mIsScoped && !destinationElementIds.contains(mId)) {
            return mExitCondition;
        } else {
            return null;
        }
    }
}
