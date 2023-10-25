// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.supplier;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;

import java.util.function.Function;

/**
 * Useful when two observable suppliers are chained together. The client class may only want to know
 * the value of the second, or "target", supplier. But to track this the client needs to observe the
 * first, or "parent", supplier, and then [un]observe the current target. Instead this class is a
 * single observable supplier that holds the current target value, greatly simplifying the client's
 * job.
 *
 * <p>Attempts to only maintain observers on the relevant observers when there's an observer on this
 * class. Clients should still remove themselves as observers from this class when done.
 *
 * @param <P> The parent object that's holding a reference to the target.
 * @param <T> The target type that the client wants to observe.
 */
public class TransitiveObservableSupplier<P, T> implements ObservableSupplier<T> {
    // Used to hold observers and current state. However the current value is only valid when there
    // are observers, otherwise is may be stale.
    private final @NonNull ObservableSupplierImpl<T> mDelegateSupplier =
            new ObservableSupplierImpl<>();

    private final @NonNull Callback<P> mOnParentSupplierChangeCallback =
            this::onParentSupplierChange;
    private final @NonNull Callback<T> mOnTargetSupplierChangeCallback =
            this::onTargetSupplierChange;
    private final @NonNull ObservableSupplier<P> mParentSupplier;
    private final @NonNull Function<P, ObservableSupplier<T>> mUnwrapFunction;

    // When this is set, then mOnTargetSupplierChangeCallback is an observer of the object
    // referenced by mCurrentTargetSupplier. When this value is changed, the observer must be
    // removed.
    private @Nullable ObservableSupplier<T> mCurrentTargetSupplier;

    public TransitiveObservableSupplier(
            ObservableSupplier<P> parentSupplier,
            Function<P, ObservableSupplier<T>> unwrapFunction) {
        mParentSupplier = parentSupplier;
        mUnwrapFunction = unwrapFunction;
    }

    @Override
    public T addObserver(Callback<T> obs) {
        if (!mDelegateSupplier.hasObservers()) {
            onParentSupplierChange(mParentSupplier.addObserver(mOnParentSupplierChangeCallback));
        }
        return mDelegateSupplier.addObserver(obs);
    }

    @Override
    public void removeObserver(Callback<T> obs) {
        mDelegateSupplier.removeObserver(obs);
        if (!mDelegateSupplier.hasObservers()) {
            mParentSupplier.removeObserver(mOnParentSupplierChangeCallback);
            if (mCurrentTargetSupplier != null) {
                mCurrentTargetSupplier.removeObserver(mOnTargetSupplierChangeCallback);
                mCurrentTargetSupplier = null;
            }
        }
    }

    @Override
    public @Nullable T get() {
        if (mDelegateSupplier.hasObservers()) {
            return mDelegateSupplier.get();
        }

        // If we have no observers, the value stored by mDelegateSupplier might not be current.
        P parentValue = mParentSupplier.get();
        if (parentValue != null) {
            ObservableSupplier<T> targetSupplier = mUnwrapFunction.apply(parentValue);
            if (targetSupplier != null) {
                return targetSupplier.get();
            }
        }
        return null;
    }

    /**
     * Conceptually this just removes our observer from the old target supplier, and our observer to
     * to the new target supplier. In practice this is full of null checks. We also have to make
     * sure we keep our delegate supplier's value up to date, which is also what drives client
     * observations.
     */
    private void onParentSupplierChange(@Nullable P parentValue) {
        if (mCurrentTargetSupplier != null) {
            mCurrentTargetSupplier.removeObserver(mOnTargetSupplierChangeCallback);
        }

        // Keep track of the current target supplier, because if this ever changes, we'll need to
        // remove our observer from it.
        mCurrentTargetSupplier = parentValue == null ? null : mUnwrapFunction.apply(parentValue);

        if (mCurrentTargetSupplier == null) {
            onTargetSupplierChange(null);
        } else {
            // While addObserver will call us if a value is already set, we do not want to depend on
            // that for two reasons. If there is no value set, we need to null out our supplier now.
            // And if there is a value set, we're going to get invoked asynchronously, which means
            // our delegate supplier could be in an intermediately incorrect state. By just setting
            // our delegate eagerly we avoid both problems.
            onTargetSupplierChange(
                    mCurrentTargetSupplier.addObserver(mOnTargetSupplierChangeCallback));
        }
    }

    private void onTargetSupplierChange(@Nullable T targetValue) {
        mDelegateSupplier.set(targetValue);
    }
}
