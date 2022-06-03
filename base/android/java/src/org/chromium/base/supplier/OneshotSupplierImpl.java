// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.supplier;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.Promise;
import org.chromium.base.ThreadUtils;

/**
 * Concrete implementation of {@link OneshotSupplier} to be used by classes owning a
 * OneshotSupplier and providing it as a dependency to others.
 *
 * <p>Instances of this class must only be accessed from the thread they were created on.
 *
 * To use:
 * <ol>
 *    <li>Create a new {@code OneshotSupplierImpl<T>} to pass as a dependency.
 *    <li>Call {@link #set(Object)} when the object becomes available. {@link #set(Object)} may only
 * be called once.
 * </ol>
 *
 * @param <T> The type of the wrapped object.
 */
public class OneshotSupplierImpl<T> implements OneshotSupplier<T> {
    private final Promise<T> mPromise = new Promise<>();
    private final ThreadUtils.ThreadChecker mThreadChecker = new ThreadUtils.ThreadChecker();

    @Override
    public T onAvailable(Callback<T> callback) {
        mThreadChecker.assertOnValidThread();
        mPromise.then(callback);
        return get();
    }

    @Override
    public @Nullable T get() {
        mThreadChecker.assertOnValidThread();
        return mPromise.isFulfilled() ? mPromise.getResult() : null;
    }

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
