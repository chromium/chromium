// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import androidx.annotation.Nullable;

import java.util.Set;
import java.util.concurrent.Callable;

/**
 * Represents a logical expression that has to be true to consider the Station active and false to
 * consider the Station exited.
 *
 * <p>LogicalElements should be declared by calling {@link
 * Elements.Builder#declareLogicalElement(LogicalElement)} passing in an instance created by one of
 * the factory methods here such as {@link #uiThreadLogicalElement(String, Callable)}.
 *
 * <p>Generates ENTER and EXIT Conditions for the ConditionalState to ensure the LogicalElement is
 * in the right state.
 *
 * <p>LogicalElements that have no Exit condition should simply be enter Conditions, declared with
 * {@link Elements.Builder#declareEnterCondition(Condition)}.
 */
public class LogicalElement implements ElementInState {
    private final boolean mIsRunOnUiThread;
    private final String mDescription;
    private final String mId;
    private final Condition mEnterCondition;
    private Condition mExitCondition;

    /**
     * Create a LogicalElement that runs the check on the UI Thread.
     *
     * <p>LogicalElements wait for the function to be true as an ENTER Condition. They also wait for
     * the function to be false as an EXIT Condition when transitioning to a ConditionalState that
     * does not declare the same LogicalElement.
     */
    public static LogicalElement uiThreadLogicalElement(
            String description, Callable<Boolean> checkFunction, String id) {
        return new LogicalElement(/* isRunOnUiThread= */ true, description, checkFunction, id);
    }

    /**
     * Version of {@link #uiThreadLogicalElement(String, Callable, String)} using the |description|
     * as |id|.
     */
    public static LogicalElement uiThreadLogicalElement(
            String description, Callable<Boolean> checkFunction) {
        return new LogicalElement(
                /* isRunOnUiThread= */ true, description, checkFunction, /* id= */ null);
    }

    /**
     * Create a LogicalElement that runs the check on the Instrumentation Thread.
     *
     * <p>LogicalElements wait for the function to be true as an ENTER Condition. They also wait for
     * the function to be false as an EXIT Condition when transitioning to a ConditionalState that
     * does not declare the same LogicalElement.
     */
    public static LogicalElement instrumentationThreadLogicalElement(
            String description, Callable<Boolean> checkFunction, String id) {
        return new LogicalElement(/* isRunOnUiThread= */ false, description, checkFunction, id);
    }

    /**
     * Version of {@link #instrumentationThreadLogicalElement(String, Callable, String)} using the
     * |description| as |id|.
     */
    public static LogicalElement instrumentationThreadLogicalElement(
            String description, Callable<Boolean> checkFunction) {
        return new LogicalElement(
                /* isRunOnUiThread= */ false, description, checkFunction, /* id= */ null);
    }

    LogicalElement(
            boolean isRunOnUiThread,
            String description,
            Callable<Boolean> checkFunction,
            @Nullable String id) {
        mIsRunOnUiThread = isRunOnUiThread;
        mDescription = description;
        mId = "LE/" + (id != null ? id : description);

        mEnterCondition =
                new Condition(mIsRunOnUiThread) {
                    @Override
                    protected ConditionStatus checkWithSuppliers() throws Exception {
                        return whether(checkFunction.call());
                    }

                    @Override
                    public String buildDescription() {
                        return "True: " + mDescription;
                    }
                };

        mExitCondition =
                new Condition(mIsRunOnUiThread) {
                    @Override
                    protected ConditionStatus checkWithSuppliers() throws Exception {
                        return whether(!checkFunction.call());
                    }

                    @Override
                    public String buildDescription() {
                        return "False: " + mDescription;
                    }
                };
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
    public Condition getExitCondition(Set<String> destinationElementIds) {
        if (!destinationElementIds.contains(mId)) {
            return mExitCondition;
        } else {
            return null;
        }
    }
}
