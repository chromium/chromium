// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import java.util.function.Function;
import java.util.function.Supplier;

/** Simple Conditions to be created from lambdas instead of subclassing. */
public class SimpleConditions {
    /** Create a simple Condition declared from a lambda ran on the instrumentation thread. */
    public static Condition instrumentationThreadCondition(
            String description, Supplier<ConditionStatus> checkFunction) {
        return new SimpleCondition(/* isRunOnUiThread= */ false, description, checkFunction);
    }

    /** Create a simple Condition declared from a lambda ran on the UI thread. */
    public static Condition uiThreadCondition(
            String description, Supplier<ConditionStatus> checkFunction) {
        return new SimpleCondition(/* isRunOnUiThread= */ true, description, checkFunction);
    }

    /**
     * Create a simple Condition that requires another Element/Condition to be checked, from a
     * lambda ran on the instrumentation thread.
     */
    public static <InputT> Condition instrumentationThreadCondition(
            String description,
            Supplier<InputT> inputElement,
            Function<InputT, ConditionStatus> checkFunction) {
        return new SimpleConditionWithInput<>(
                /* isRunOnUiThread= */ false, description, inputElement, checkFunction);
    }

    /**
     * Create a simple {@link ConditionWithResult} that requires another Element/Condition to be
     * checked, from a lambda ran on the UI thread.
     */
    public static <InputT, OutputT> ConditionWithResult<OutputT> uiThreadConditionWithResult(
            String description,
            Supplier<InputT> inputElement,
            Function<InputT, ConditionStatusWithResult<OutputT>> checkFunction) {
        return new SimpleConditionWithInputAndOutput<>(
                /* isRunOnUiThread= */ true, description, inputElement, checkFunction);
    }

    /**
     * Create a simple {@link ConditionWithResult} that requires another Element/Condition to be
     * checked, from a lambda ran on the instrumentation thread.
     */
    public static <InputT, OutputT>
            ConditionWithResult<OutputT> instrumentationThreadConditionWithResult(
                    String description,
                    Supplier<InputT> inputElement,
                    Function<InputT, ConditionStatusWithResult<OutputT>> checkFunction) {
        return new SimpleConditionWithInputAndOutput<>(
                /* isRunOnUiThread= */ false, description, inputElement, checkFunction);
    }

    /**
     * Create a simple Condition that requires another Element/Condition to be checked, from a
     * lambda ran on the UI thread.
     */
    public static <InputT> Condition uiThreadCondition(
            String description,
            Supplier<InputT> inputElement,
            Function<InputT, ConditionStatus> checkFunction) {
        return new SimpleConditionWithInput<>(
                /* isRunOnUiThread= */ true, description, inputElement, checkFunction);
    }

    /** A simple Condition declared from a lambda without subclassing. */
    private static class SimpleCondition extends Condition {
        private final String mDescription;
        private final Supplier<ConditionStatus> mCheckFunction;

        private SimpleCondition(
                boolean isRunOnUiThread,
                String description,
                Supplier<ConditionStatus> checkFunction) {
            super(isRunOnUiThread);
            mDescription = description;
            mCheckFunction = checkFunction;
        }

        @Override
        protected ConditionStatus checkWithSuppliers() {
            return mCheckFunction.get();
        }

        @Override
        public String buildDescription() {
            return mDescription;
        }
    }

    /**
     * A simple Condition declared from a lambda that requires another Element/Condition to be
     * checked.
     */
    private static class SimpleConditionWithInput<InputT> extends Condition {
        private final String mDescription;
        private final Supplier<InputT> mInputSupplier;
        private final Function<InputT, ConditionStatus> mCheckFunction;

        private SimpleConditionWithInput(
                boolean isRunOnUiThread,
                String description,
                Supplier<InputT> inputSupplier,
                Function<InputT, ConditionStatus> checkFunction) {
            super(isRunOnUiThread);
            mDescription = description;
            mInputSupplier = dependOnSupplier(inputSupplier, "Input");
            mCheckFunction = checkFunction;
        }

        @Override
        protected ConditionStatus checkWithSuppliers() {
            return mCheckFunction.apply(mInputSupplier.get());
        }

        @Override
        public String buildDescription() {
            return mDescription;
        }
    }

    /**
     * A simple ConditionWithResult declared from a lambda that requires another Element/Condition
     * to be checked.
     */
    private static class SimpleConditionWithInputAndOutput<InputT, OutputT>
            extends ConditionWithResult<OutputT> {
        private final String mDescription;
        private final Supplier<InputT> mInputSupplier;
        private final Function<InputT, ConditionStatusWithResult<OutputT>> mCheckFunction;

        private SimpleConditionWithInputAndOutput(
                boolean isRunOnUiThread,
                String description,
                Supplier<InputT> inputSupplier,
                Function<InputT, ConditionStatusWithResult<OutputT>> checkFunction) {
            super(isRunOnUiThread);
            mDescription = description;
            mInputSupplier = dependOnSupplier(inputSupplier, "Input");
            mCheckFunction = checkFunction;
        }

        @Override
        protected ConditionStatusWithResult<OutputT> resolveWithSuppliers() throws Exception {
            return mCheckFunction.apply(mInputSupplier.get());
        }

        @Override
        public String buildDescription() {
            return mDescription;
        }
    }
}
