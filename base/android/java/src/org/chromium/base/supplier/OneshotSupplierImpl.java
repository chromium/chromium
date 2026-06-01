// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.supplier;

import org.chromium.base.Callback;
import org.chromium.base.Promise;
import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

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
@NullMarked
public class OneshotSupplierImpl<T> implements OneshotSupplier<T> {
    private final Promise<T> mPromise = new Promise<>();
    private final ThreadUtils.ThreadChecker mThreadChecker = new ThreadUtils.ThreadChecker();

    public OneshotSupplierImpl() {
        // Guard against creation on Instrumentation thread, since this causes the ThreadChecker
        // to be associated with it (it should be UI thread).
        assert !ThreadUtils.runningOnInstrumentationThread();
    }

    @Override
    public @Nullable T onAvailable(Callback<T> callback) {
        mThreadChecker.assertOnValidOrInstrumentationThread();
        mPromise.then(callback);
        return get();
    }

    @Override
    public @Nullable T get() {
        mThreadChecker.assertOnValidOrInstrumentationThread();
        return mPromise.isFulfilled() ? mPromise.getResult() : null;
    }

    /**
     * Set the object supplied by this supplier. This will post notifications to registered
     * callbacks that the dependency is available. If set() has already been called, this method
     * will assert.
     *
     * @param object The object to supply.
     */
    public void set(T object) {
        mThreadChecker.assertOnValidOrInstrumentationThread();
        assert !mPromise.isFulfilled();
        assert object != null;
        mPromise.fulfill(object);
    }
}
