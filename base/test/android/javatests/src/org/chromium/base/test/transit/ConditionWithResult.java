// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import org.chromium.base.supplier.Supplier;

/**
 * A {@link Condition} which supplies {@param <ResultT>} when fulfilled.
 *
 * @param <ResultT> The type of the result supplied when the condition is fulfilled.
 */
public abstract class ConditionWithResult<ResultT> extends Condition implements Supplier<ResultT> {

    private ResultT mResult;

    public ConditionWithResult(boolean isRunOnUiThread) {
        super(isRunOnUiThread);
    }

    @Override
    public ResultT get() {
        return mResult;
    }

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
