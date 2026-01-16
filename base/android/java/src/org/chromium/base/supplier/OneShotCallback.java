// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.supplier;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullUnmarked;

import java.lang.ref.WeakReference;
import java.util.function.Supplier;

/**
 * Helper class to handle safely querying a single instance of an object from an {@link
 * MonotonicObservableSupplier}.
 *
 * <p>Assuming the underlying {@link Supplier} gets set with a value, this class will guarantee only
 * a single call makes it back to the passed in {@link Callback}.
 *
 * <p>For {@link MonotonicObservableSupplier}s that already have a valid value set, this will have the same
 * underlying behavior as {@link ObservableSupplierImpl}, which asynchronously triggers the callback
 * when {@link MonotonicObservableSupplier#addObserver(Callback)} is called.
 *
 * <p>This class does not hold a strong reference to the {@link MonotonicObservableSupplier}, but does hold a
 * strong reference to the {@link Callback}.
 *
 * @param <T> The type of the wrapped object.
 */
@NullUnmarked // Callback can be nullable or non-nullable.
public class OneShotCallback<T> {
    private final Callback<T> mCallbackWrapper = new CallbackWrapper();
    private final WeakReference<NullableObservableSupplier<T>> mWeakSupplier;
    private final Callback<T> mCallback;

    /**
     * Creates a {@link OneShotCallback} instance, automatically registering as an observer to
     * {@code supplier} and waiting to trigger {@code callback}.
     *
     * @param supplier The {@link MonotonicObservableSupplier} to wait for.
     * @param callback The {@link Callback} to notify with a valid value.
     */
    public OneShotCallback(NullableObservableSupplier<T> supplier, Callback<T> callback) {
        mWeakSupplier = new WeakReference<>(supplier);
        mCallback = callback;
        supplier.addObserver(mCallbackWrapper);
    }

    private class CallbackWrapper implements Callback<T> {
        @Override
        public void onResult(T result) {
            mCallback.onResult(result);
            NullableObservableSupplier<T> supplier = mWeakSupplier.get();
            assert supplier != null
                    : "This can only be called by supplier, which should not be null.";
            supplier.removeObserver(mCallbackWrapper);
        }
    }
}
