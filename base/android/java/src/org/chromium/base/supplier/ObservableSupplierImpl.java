// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.supplier;

import org.chromium.base.Callback;
import org.chromium.base.ObserverList;
import org.chromium.base.ThreadUtils;
import org.chromium.base.ThreadUtils.ThreadChecker;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.build.annotations.RequiresNonNull;

import java.util.Objects;
import java.util.function.Supplier;

/**
 * Implementation for Settable{NonNull|Monotonic}ObservableSupplier.
 *
 * <p>Since this class is both nullable and non-null, it should only be used directly when needing
 * to create subclasses. All normal uses should be done through interface types; creation should be
 * done via ObservableSuppliers.
 *
 * <pre>
 * Some implementation details:
 *   * Must only be accessed from the UI thread.
 *   * Callbacks from set() are executed synchronously (not posted).
 *   * Callbacks from addSyncObserverAndPostIfNonNull() are automatically cancelled if the observer
 *     is removed or the value is changed before they are run.
 * </pre>
 */
@NullMarked
@SuppressWarnings("NullAway") // Implementation for both Nullable and NonNull.
class ObservableSupplierImpl<T> extends BaseObservableSupplierImpl<T>
        implements Supplier<T>,
                SettableNullableObservableSupplier<T>,
                SettableMonotonicObservableSupplier<T>,
                SettableNonNullObservableSupplier<T> {
    protected final ThreadChecker mThreadChecker = new ThreadChecker();
    protected @Nullable ObserverList<Callback<T>> mObservers;
    protected T mObject;
    private boolean mIsDestroyed;

    protected ObservableSupplierImpl(@Nullable T initialValue, boolean allowSetToNull) {
        super(allowSetToNull);
        mObject = initialValue;
        // Guard against creation on Instrumentation thread, since this causes the ThreadChecker
        // to be associated with it (it should be UI thread).
        assert !ThreadUtils.runningOnInstrumentationThread();
    }

    @Override
    public T addObserver(Callback<T> obs, @NotifyBehavior int behavior) {
        assert !mIsDestroyed : "addObserver called on destroyed supplier";
        if (mIsDestroyed) {
            return null;
        }
        if (mObservers == null) {
            mObservers = new ObserverList<>();
        }
        // ObserverList has its own ThreadChecker.
        mObservers.addObserver(obs);

        T currentObject = mObject;
        boolean notify =
                shouldNotifyOnAdd(behavior)
                        && (currentObject != null || shouldAllowNullOnAdd(behavior));
        if (notify) {
            if (shouldPostOnAdd(behavior)) {
                ThreadUtils.assertOnUiThread();
                ThreadUtils.postOnUiThread(
                        () -> {
                            if (mObject == currentObject
                                    && mObservers != null
                                    && mObservers.hasObserver(obs)) {
                                obs.onResult(currentObject);
                            }
                        });
            } else {
                obs.onResult(currentObject);
            }
        }

        return currentObject;
    }

    @Override
    public void removeObserver(Callback<T> obs) {
        // Check not destroyed.
        if (mObservers != null) {
            mObservers.removeObserver(obs);
        }
    }

    @Override
    public void set(T object) {
        // Sometimes ObservableSupplierImpl::set is linked directly to an event that does not get
        // destroyed, so it's easier to ignore set() after destroy() than to have callers have to
        // track the state. It can also be hard to ensure queued callbacks that call set() are
        // cancelled, so again, just ignore after destroy().
        if (!mIsDestroyed) {
            mThreadChecker.assertOnValidThread();
            assert object != null || mAllowSetToNull
                    : "set(null) called on a non-nullable supplier";
            T prevValue = mObject;
            mObject = object;
            if (mObservers != null) {
                callObservers(prevValue);
            }
        }
    }

    @Override
    @SuppressWarnings("NullAway")
    public void destroy() {
        mIsDestroyed = true;
        mObservers = null;
        mObject = null;
    }

    /* package */ boolean isDestroyed() {
        return mIsDestroyed;
    }

    @RequiresNonNull("mObservers")
    private void callObservers(T prevValue) {
        T value = mObject;
        if (Objects.equals(prevValue, value)) {
            return;
        }
        for (Callback<T> observer : mObservers) {
            observer.onResult(value);
        }
    }

    @Override
    public T get() {
        // Allow instrumentation thread access since tests often access variables for asserts.
        // https://crbug.com/1173814
        mThreadChecker.assertOnValidOrInstrumentationThread();
        return mObject;
    }

    /** Returns if there are any observers currently. */
    @Override
    public int getObserverCount() {
        return mObservers == null ? 0 : mObservers.size();
    }

    /** Returns whether the observer should be notified on being added. */
    private static boolean shouldNotifyOnAdd(@NotifyBehavior int behavior) {
        return (NotifyBehavior.NOTIFY_ON_ADD & behavior) != 0;
    }

    /** Returns whether the observer should be notified asynchronously on being added. */
    private static boolean shouldPostOnAdd(@NotifyBehavior int behavior) {
        return (NotifyBehavior.POST_ON_ADD & behavior) != 0;
    }

    /** Returns whether the observer should be notified on being added even if value is null. */
    private static boolean shouldAllowNullOnAdd(@NotifyBehavior int behavior) {
        return (NotifyBehavior.ALLOW_NULL_ON_ADD & behavior) != 0;
    }
}
