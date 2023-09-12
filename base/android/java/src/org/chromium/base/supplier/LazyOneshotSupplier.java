// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.supplier;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.Promise;
import org.chromium.base.ThreadUtils;

/**
 * Abstract lazy implementation of {@link OneshotSupplier} to be used by classes owning a
 * OneshotSupplier and providing it as a dependency to others. This class differs from
 * {@link OneshotSupplierImpl} in that a call to {@link LazyOneshotSupplier#get()} will attempt to
 * set the supplied object via {@link LazyOneshotSupplier#doSet()}. Additionally,
 * {@link LazyOneshotSupplier#onAvailable(Callback<T>)} will not call
 * {@link LazyOneshotSupplier#get()} unless it already has a value to prevent eager initialization.
 *
 * If eager initialization in response to {@link LazyOneshotSupplier#onAvailable(Callback<T>)} is
 * required then a call to {@link LazyOneshotSupplier#get()} can be made just before attaching
 * the callback.
 *
 * <p>Instances of this class must only be accessed from the thread they were created on.
 *
 * To use:
 * <ol>
 *    <li>Create a new {@code LazyOneshotSupplier<T>} to pass as a dependency.
 *    <li>Override {@link #doSet()} to invoke {@link #set(T)}. This will be invoked when
 *        {@link #get()} is invoked if {@link #hasValue()} returns false. Note that invoking
 *        {@link #doSet()} does not have to invoke {@link #set(T)} if there is reason not to
 *        such as awaiting an async dependency. However, if this is the case clients of the
 *        supplier need to be careful to properly understand the initialization lifecycle.
 * </ol>
 *
 * @param <T> The type of the wrapped object.
 */
public abstract class LazyOneshotSupplier<T> implements OneshotSupplier<T> {
    private final Promise<T> mPromise = new Promise<>();
    private final ThreadUtils.ThreadChecker mThreadChecker = new ThreadUtils.ThreadChecker();

    /**
     * Lazily invokes the callback the first time {@link #set(T)} is
     * invoked or immediately if already available.
     */
    @Override
    public T onAvailable(Callback<T> callback) {
        mThreadChecker.assertOnValidThread();
        mPromise.then(callback);
        return hasValue() ? get() : null;
    }

    /**
     * Return the value of the supplier. Calling this the first time will initialize the
     * value in the supplier via {@link #doSet()}.
     * @return the value that was provided in {@link #set(T)} or null.
     */
    @Override
    public @Nullable T get() {
        mThreadChecker.assertOnValidThread();
        if (!hasValue()) {
            doSet();
        }
        return hasValue() ? mPromise.getResult() : null;
    }

    /**
     * Returns whether a value is set in the supplier.
     */
    @Override
    public boolean hasValue() {
        return mPromise.isFulfilled();
    }

    /**
     * Sets the value upon first {@link #get()}. Implementers should override this to invoke
     * {@link #set(T)}.
     */
    public abstract void doSet();

    /**
     * Set the object supplied by this supplier. This will notify registered callbacks that the
     * dependency is available. If set() has already been called, this method will assert.
     *
     * @param object The object to supply.
     */
    public void set(@NonNull T object) {
        mThreadChecker.assertOnValidThread();
        assert !mPromise.isFulfilled();
        assert object != null;
        mPromise.fulfill(object);
    }
}
