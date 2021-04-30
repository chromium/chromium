// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.supplier;

import android.os.Handler;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.ObserverList;
import org.chromium.base.ThreadUtils;
import org.chromium.base.ThreadUtils.ThreadChecker;
import org.chromium.base.lifecycle.DestroyChecker;
import org.chromium.base.lifecycle.Destroyable;

/**
 * Concrete implementation of {@link ObservableSupplier} to be used by classes owning the
 * ObservableSupplier and providing it as a dependency to others.
 *
 * This class must only be accessed from a single thread, which is enforced by
 * {@link ThreadUtils.ThreadChecker}.
 *
 * To use:
 *   1. Create a new ObservableSupplierImpl<E> to pass as a dependency
 *   2. Call {@link #set(Object)} when the real object becomes available. {@link #set(Object)} may
 *      be called multiple times. Observers will be notified each time a new object is set. In case
 *      where a supplied object implements {@link Destroyable}, it will be destroyed, when it is
 *      replaced by another one in supplier.
 *   3. The class implements {@link Destroyable} and is meant to be destroyed, when the owning class
 *      is being destroyed. When it is destroyed, it will also destroy the supplied object.
 *
 * @param <E> The type of the wrapped object.
 */
public class ObservableSupplierImpl<E> implements ObservableSupplier<E>, Destroyable {
    private final ThreadChecker mThreadChecker = new ThreadChecker();
    private final DestroyChecker mDestroyChecker = new DestroyChecker();
    private final Handler mHandler = new Handler();

    private E mObject;
    private final ObserverList<Callback<E>> mObservers = new ObserverList<>();

    @Override
    public E addObserver(Callback<E> obs) {
        checkState();
        mObservers.addObserver(obs);

        if (mObject != null) {
            final E currentObject = mObject;
            mHandler.post(() -> {
                if (mObject != currentObject || !mObservers.hasObserver(obs)) return;
                obs.onResult(mObject);
            });
        }

        return mObject;
    }

    @Override
    public void removeObserver(Callback<E> obs) {
        checkState();
        mObservers.removeObserver(obs);
    }

    /**
     * Set the object supplied by this supplier. This will notify registered callbacks that the
     * dependency is available.
     * @param object The object to supply.
     */
    public void set(E object) {
        checkState();
        if (object == mObject) return;

        maybeDestroyHeldObject();
        mObject = object;

        for (Callback<E> observer : mObservers) {
            observer.onResult(mObject);
        }
    }

    @Override
    public @Nullable E get() {
        checkState();
        return mObject;
    }

    @Override
    public void destroy() {
        mThreadChecker.assertOnValidThread();
        maybeDestroyHeldObject();
        mObject = null;
        mDestroyChecker.destroy();
    }

    private void maybeDestroyHeldObject() {
        if (mObject instanceof Destroyable) {
            ((Destroyable) mObject).destroy();
        }
    }

    private void checkState() {
        mThreadChecker.assertOnValidThread();
        mDestroyChecker.checkNotDestroyed();
    }

    /** Used to allow developers to access supplier values on the instrumentation thread. */
    @VisibleForTesting
    public static void setIgnoreThreadChecksForTesting(boolean ignoreThreadChecks) {
        ThreadUtils.setThreadAssertsDisabledForTesting(ignoreThreadChecks); // IN-TEST
    }
}
