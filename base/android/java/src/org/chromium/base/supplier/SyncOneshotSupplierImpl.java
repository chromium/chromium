// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.supplier;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;

import java.util.ArrayList;

/**
 * A concrete implementation of {@link SyncOneshotSupplier} to used when callbacks need to be called
 * synchronously when a value is set. This differs from {@link OneshotSupplierImpl} in that the
 * pending {@link Callback}s are not posted when {@link #set(T)} is invoked.
 *
 * <p>Instances of this class must only be accessed from the thread they were created on.
 *
 * <p>To use:
 *
 * <ol>
 *   <li>Create a new {@code SyncOneshotSupplierImpl<T>} to pass as a dependency.
 *   <li>Call {@link #set(Object)} when the object becomes available. {@link #set(Object)} may only
 *       be called once.
 * </ol>
 *
 * @param <T> The type of the wrapped object.
 */
public class SyncOneshotSupplierImpl<T> implements SyncOneshotSupplier<T> {
    private final ThreadUtils.ThreadChecker mThreadChecker = new ThreadUtils.ThreadChecker();

    /** Lazily created list of pending callbacks to invoke when an object is set. */
    private @Nullable ArrayList<Callback<T>> mPendingCallbacks;

    private @Nullable T mObject;

    @Override
    public @Nullable T onAvailable(Callback<T> callback) {
        mThreadChecker.assertOnValidThread();
        T object = get();
        if (object != null) {
            callback.onResult(object);
        } else {
            if (mPendingCallbacks == null) {
                mPendingCallbacks = new ArrayList<Callback<T>>();
            }
            mPendingCallbacks.add(callback);
        }
        return object;
    }

    @Override
    public @Nullable T get() {
        mThreadChecker.assertOnValidThread();
        return mObject;
    }

    /**
     * Set the object supplied by this supplier. This will synchronously notify registered callbacks
     * that the dependency is available. If {@link #set(Object)} has already been called, this
     * method will assert.
     *
     * @param object The object to supply.
     */
    public void set(@NonNull T object) {
        mThreadChecker.assertOnValidThread();
        assert mObject == null;
        assert object != null;
        mObject = object;
        if (mPendingCallbacks == null) return;

        for (Callback<T> callback : mPendingCallbacks) {
            callback.onResult(object);
        }
        mPendingCallbacks = null;
    }
}
