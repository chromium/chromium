// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.supplier;

import androidx.annotation.NonNull;

import org.chromium.base.Callback;

import java.lang.ref.WeakReference;

/**
 * Helper class to handle safely querying a single instance of an object from an
 * {@link ObservableSupplier}.
 *
 * Assuming the underlying {@link Supplier} gets set with a
 * value, this class will guarantee only a single call makes it back to the passed in
 * {@link Callback}.
 *
 * For {@link ObservableSupplier}s that already have a valid value set, this will have the same
 * underlying behavior as {@link ObservableSupplierImpl}, which asynchronously triggers the callback
 * when {@link ObservableSupplier#addObserver(Callback)} is called.
 *
 * This class does not hold a strong reference to the {@link ObservableSupplier}, but does hold a
 * strong reference to the {@link Callback}.
 *
 * @param <E> The type of the wrapped object.
 */
public class OneShotCallback<E> {
    private final Callback<E> mCallbackWrapper = new CallbackWrapper();
    private final WeakReference<ObservableSupplier<E>> mWeakSupplier;
    private final Callback<E> mCallback;

    /**
     * Creates a {@link OneShotCallback} instance, automatically registering as an observer to
     * {@code supplier} and waiting to trigger {@code callback}.
     * @param supplier The {@link ObservableSupplier} to wait for.
     * @param callback The {@link Callback} to notify with a valid value.
     */
    public OneShotCallback(@NonNull ObservableSupplier<E> supplier, @NonNull Callback<E> callback) {
        mWeakSupplier = new WeakReference<>(supplier);
        mCallback = callback;

        supplier.addObserver(mCallbackWrapper);
    }

    private class CallbackWrapper implements Callback<E> {
        @Override
        public void onResult(E result) {
            mCallback.onResult(result);
            ObservableSupplier<E> supplier = mWeakSupplier.get();
            assert supplier
                    != null : "This can only be called by supplier, which should not be null.";
            supplier.removeObserver(mCallbackWrapper);
        }
    }
}