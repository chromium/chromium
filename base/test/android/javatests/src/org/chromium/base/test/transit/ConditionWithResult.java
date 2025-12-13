// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.function.Supplier;

/**
 * A {@link Condition} which supplies {@param <ResultT>} when fulfilled.
 *
 * @param <ResultT> The type of the result supplied when the condition is fulfilled.
 */
@NullMarked
public abstract class ConditionWithResult<ResultT> extends Condition
        implements Supplier<@Nullable ResultT> {

    private @Nullable ResultT mResult;

    public ConditionWithResult(boolean isRunOnUiThread) {
        super(isRunOnUiThread);
    }

    /**
     * @return the result of the Condition (View, Activity, etc.), or null if the Condition is not
     *     fulfilled.
     * @throws AssertionError if any of:
     *     <pre>
     *     1) the Condition was neither bound to a ConditionalState nor to a Transition
     *     2) the Condition is bound to a ConditionalState that's NEW or FINISHED
     *     </pre>
     */
    @Override
    public @Nullable ResultT get() {
        assertIsBound();
        if (mOwnerState != null) {
            mOwnerState.assertSuppliersMightBeValid();
        }
        return mResult;
    }

    /**
     * Same as get() but callable after the ConditionalState is transitioned from. Use with caution,
     * as most of the time this means the product is not usable anymore.
     *
     * @return the result of the Condition (View, Activity, etc.) from a non-NEW ConditionalState
     */
    public ResultT getFromPast() {
        assertIsBound();
        ResultT result = mResult;
        assert result != null : String.format("Condition \"%s\" has null result", getDescription());
        return result;
    }

    // Condition implementation
    @Override
    protected final ConditionStatus checkWithSuppliers() throws Exception {
        ConditionStatusWithResult<ResultT> statusWithResult = resolveWithSuppliers();
        if (statusWithResult.getResult() != null) {
            mResult = statusWithResult.getResult();
        }
        return statusWithResult.getStatus();
    }

    /**
     * Same as {@link Condition#checkWithSuppliers()}, plus possibly produce a {@param <ResultT>}.
     *
     * <p>Depending on #shouldRunOnUiThread(), called on the UI or the instrumentation thread.
     *
     * @return {@link ConditionStatusWithResult} stating whether the condition has been fulfilled,
     *     the produced result, and optionally more details about its state.
     */
    protected abstract ConditionStatusWithResult<ResultT> resolveWithSuppliers() throws Exception;
}
