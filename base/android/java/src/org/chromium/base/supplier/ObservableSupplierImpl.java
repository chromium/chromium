// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.supplier;

import static org.chromium.build.NullUtil.assumeNonNull;

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
// TODO(455874046): Remove "T extends @Nullable Object".
public class ObservableSupplierImpl<T extends @Nullable Object>
        extends BaseObservableSupplierImpl<T>
        implements Supplier<T>,
                SettableNullableObservableSupplier<T>,
                SettableMonotonicObservableSupplier<T>,
                SettableNonNullObservableSupplier<T> {
    protected final ThreadChecker mThreadChecker = new ThreadChecker();
    protected @Nullable ObserverList<Callback<T>> mObservers = new ObserverList<>();
    protected T mObject;

    @Deprecated // Migrate to ObservableSuppliers.*
    public ObservableSupplierImpl() {
        this(null, /* allowSetToNull= */ null);
    }

    @Deprecated // Migrate to ObservableSuppliers.*
    public ObservableSupplierImpl(T initialValue) {
        this(initialValue, /* allowSetToNull= */ null);
    }

    protected ObservableSupplierImpl(@Nullable T initialValue, @Nullable Boolean allowSetToNull) {
        super(allowSetToNull);
        mObject = initialValue;
        // Guard against creation on Instrumentation thread, since this causes the ThreadChecker
        // to be associated with it (it should be UI thread).
        assert !ThreadUtils.runningOnInstrumentationThread();
    }

    @Override
    public T addObserver(Callback<T> obs, @NotifyBehavior int behavior) {
        assumeNonNull(mObservers); // Check not destroyed.
        // ObserverList has its own ThreadChecker.
        mObservers.addObserver(obs);

        T currentObject = mObject;
        boolean notify = shouldNotifyOnAdd(behavior) && currentObject != null;
        if (notify) {
            if (shouldPostOnAdd(behavior)) {
                ThreadUtils.assertOnUiThread();
                ThreadUtils.postOnUiThread(
                        () -> {
                            if (mObject == currentObject && mObservers.hasObserver(obs)) {
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
        if (mObservers != null) {
            mThreadChecker.assertOnValidThread();
            assert object != null || !Boolean.FALSE.equals(mAllowSetToNull)
                    : "set(null) called on a non-nullable supplier";
            T prevValue = mObject;
            mObject = object;
            callObservers(prevValue);
        }
    }

    @Override
    @SuppressWarnings("NullAway")
    public void destroy() {
        mObservers = null;
        mObject = null;
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
    private static boolean shouldPostOnAdd(int behavior) {
        return (NotifyBehavior.POST_ON_ADD & behavior) != 0;
    }
}
