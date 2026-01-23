// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.supplier;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * Base class singletons in ObservableSuppliers. Class is stateless by not having observers (and so
 * also does not have thread checks).
 */
@NullMarked
@SuppressWarnings("NullAway")
class BaseObservableSupplierImpl<T extends @Nullable Object>
        implements MonotonicObservableSupplier<T> {
    // TODO(455874046): Make this non-nullable once ObservableSupplierImpl is no longer directly
    // used.
    protected @Nullable Boolean mAllowSetToNull;

    BaseObservableSupplierImpl(@Nullable Boolean allowSetToNull) {
        mAllowSetToNull = allowSetToNull;
    }

    @Override
    public T addObserver(Callback<T> obs, @NotifyBehavior int behavior) {
        return null;
    }

    @Override
    public void removeObserver(Callback<T> obs) {}

    @Override
    public T get() {
        return null;
    }

    @Override
    public int getObserverCount() {
        return 0;
    }

    /**
     * If |thing| is a BaseObservableSupplierImpl, returns mAllowSetToNull. Otherwise, returns
     * whether |thing| is an instance of ObservableSupplier. E.g. this assumes that classes that do
     * not extend BaseObservableSupplierImpl will no implement conflicting interfaces like
     * ObservableSupplierImpl does.
     */
    static <T> @Nullable Boolean allowsSetToNull(NullableObservableSupplier<T> thing) {
        if (thing instanceof BaseObservableSupplierImpl<T> impl) {
            return impl.mAllowSetToNull;
        }
        return !(thing instanceof MonotonicObservableSupplier<T>);
    }
}
