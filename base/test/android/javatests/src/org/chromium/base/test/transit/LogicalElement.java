// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import androidx.annotation.Nullable;

import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.transit.ConditionStatus.Status;

import java.util.concurrent.Callable;
import java.util.function.Function;

/**
 * Represents a logical expression that has to be true to consider the Station active and false to
 * consider the Station exited.
 *
 * <p>The logical expression is passed in as a |checkFunction|.
 *
 * <p>LogicalElements should be declared by calling {@link
 * Elements.Builder#declareLogicalElement(LogicalElement)} passing in an instance created by one of
 * the factory methods here such as {@link #uiThreadLogicalElement(String, Function, Supplier)}.
 *
 * <p>Generates ENTER and EXIT Conditions for the ConditionalState to ensure the LogicalElement is
 * in the right state.
 *
 * <p>LogicalElements that have no Exit condition should simply be enter Conditions, declared with
 * {@link Elements.Builder#declareEnterCondition(Condition)}.
 *
 * @param <ParamT> type of parameter the |checkFunction| requires.
 */
public class LogicalElement<ParamT> extends Element<Void> {

    private static final ConditionWithResult<Void> CONDITION_WITH_NULL_RESULT =
            new ConditionWithResult<>(/* isRunOnUiThread= */ false) {
                @Override
                public String buildDescription() {
                    return "Supplier of null";
                }

                @Override
                public boolean hasValue() {
                    return true;
                }

                @Override
                protected ConditionStatusWithResult<Void> resolveWithSuppliers() {
                    return fulfilled().withResult(null);
                }
            };
    private final boolean mIsRunOnUiThread;
    private final String mDescription;
    private final Function<ParamT, ConditionStatus> mCheckFunction;
    private final Supplier<ParamT> mParamSupplier;

    /**
     * Create a LogicalElement that runs the check on the UI Thread.
     *
     * <p>LogicalElements wait for the function to be true as an ENTER Condition. They also wait for
     * the function to be false as an EXIT Condition when transitioning to a ConditionalState that
     * does not declare the same LogicalElement.
     */
    public static <T> LogicalElement<T> uiThreadLogicalElement(
            String description,
            Function<T, ConditionStatus> checkFunction,
            Supplier<T> paramSupplier,
            String id) {
        return new LogicalElement<>(
                /* isRunOnUiThread= */ true, description, checkFunction, paramSupplier, id);
    }

    /**
     * Version of {@link #uiThreadLogicalElement(String, Function, Supplier, String)} using the
     * |description| as |id|.
     */
    public static <T> LogicalElement<T> uiThreadLogicalElement(
            String description,
            Function<T, ConditionStatus> checkFunction,
            Supplier<T> paramSupplier) {
        return new LogicalElement<>(
                /* isRunOnUiThread= */ true,
                description,
                checkFunction,
                paramSupplier,
                /* id= */ null);
    }

    /**
     * Version of {@link #uiThreadLogicalElement(String, Function, Supplier)} when |checkFunction|
     * has no dependencies.
     */
    public static LogicalElement<Void> uiThreadLogicalElement(
            String description, Callable<ConditionStatus> checkCallable) {
        return new LogicalElement<>(
                /* isRunOnUiThread= */ true,
                description,
                new CallableAsFunction(checkCallable),
                CONDITION_WITH_NULL_RESULT,
                /* id= */ null);
    }

    /**
     * Create a LogicalElement that runs the check on the Instrumentation Thread.
     *
     * <p>LogicalElements wait for the function to be true as an ENTER Condition. They also wait for
     * the function to be false as an EXIT Condition when transitioning to a ConditionalState that
     * does not declare the same LogicalElement.
     */
    public static <T> LogicalElement<T> instrumentationThreadLogicalElement(
            String description,
            Function<T, ConditionStatus> checkFunction,
            Supplier<T> paramSupplier,
            String id) {
        return new LogicalElement<>(
                /* isRunOnUiThread= */ false, description, checkFunction, paramSupplier, id);
    }

    /**
     * Version of {@link #instrumentationThreadLogicalElement(String, Function, Supplier, String)}
     * using the |description| as |id|.
     */
    public static <T> LogicalElement<T> instrumentationThreadLogicalElement(
            String description,
            Function<T, ConditionStatus> checkFunction,
            Supplier<T> paramSupplier) {
        return new LogicalElement<>(
                /* isRunOnUiThread= */ false,
                description,
                checkFunction,
                paramSupplier,
                /* id= */ null);
    }

    /**
     * Version of {@link #instrumentationThreadLogicalElement(String, Function, Supplier)} when
     * |checkFunction| has no dependencies.
     */
    public static LogicalElement<Void> instrumentationThreadLogicalElement(
            String description, Callable<ConditionStatus> checkCallable) {
        return new LogicalElement<>(
                /* isRunOnUiThread= */ false,
                description,
                new CallableAsFunction(checkCallable),
                CONDITION_WITH_NULL_RESULT,
                /* id= */ null);
    }

    LogicalElement(
            boolean isRunOnUiThread,
            String description,
            Function<ParamT, ConditionStatus> checkFunction,
            Supplier<ParamT> paramSupplier,
            @Nullable String id) {
        super("LE/" + (id != null ? id : description));
        mIsRunOnUiThread = isRunOnUiThread;
        mDescription = description;
        mCheckFunction = checkFunction;
        mParamSupplier = paramSupplier;
    }

    @Override
    public ConditionWithResult<Void> createEnterCondition() {
        return new EnterCondition(mIsRunOnUiThread);
    }

    @Override
    public Condition createExitCondition() {
        return new ExitCondition(mIsRunOnUiThread);
    }

    private class EnterCondition extends ConditionWithResult<Void> {
        private EnterCondition(boolean isRunOnUiThread) {
            super(isRunOnUiThread);
            dependOnSupplier(mParamSupplier, "Param");
        }

        @Override
        protected ConditionStatusWithResult<Void> resolveWithSuppliers() {
            return mCheckFunction.apply(mParamSupplier.get()).withoutResult();
        }

        @Override
        public String buildDescription() {
            return "True: " + mDescription;
        }
    }

    private class ExitCondition extends Condition {
        private ExitCondition(boolean isRunOnUiThread) {
            super(isRunOnUiThread);
            dependOnSupplier(mParamSupplier, "Param");
        }

        @Override
        protected ConditionStatus checkWithSuppliers() {
            ConditionStatus functionResult = mCheckFunction.apply(mParamSupplier.get());
            return new ConditionStatus(
                    invertStatus(functionResult.getStatus()), functionResult.getMessage());
        }

        @Override
        public String buildDescription() {
            return "False: " + mDescription;
        }
    }

    private @Status int invertStatus(@Status int status) {
        return switch (status) {
            case Status.NOT_FULFILLED -> Status.FULFILLED;
            case Status.FULFILLED -> Status.NOT_FULFILLED;
            case Status.ERROR -> Status.ERROR;
            case Status.AWAITING -> Status.AWAITING;
            default -> throw new IllegalStateException("Unexpected value: " + status);
        };
    }

    private static class CallableAsFunction implements Function<Void, ConditionStatus> {

        private final Callable<ConditionStatus> mCheckCallable;

        private CallableAsFunction(Callable<ConditionStatus> checkCallable) {
            mCheckCallable = checkCallable;
        }

        @Override
        public ConditionStatus apply(Void voidParam) {
            try {
                return mCheckCallable.call();
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }
    }
}
