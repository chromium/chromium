// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.supplier;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullUnmarked;
import org.chromium.build.annotations.Nullable;

import java.util.function.Function;

/**
 * Useful when two observable suppliers are chained together. The client class may only want to know
 * the value of the second, or "target", supplier. But to track this the client needs to observe the
 * first, or "parent", supplier, and then [un]observe the current target. Instead this class is a
 * single observable supplier that holds the current target value, greatly simplifying the client's
 * job.
 *
 * <p><ParentT>Attempts to only maintain observers on the relevant observers when there's an
 * observer on this class. Clients should still remove themselves as observers from this class when
 * done.
 *
 * @param <ParentT> The parent object that's holding a reference to the target.
 * @param <ChildT> The target type that the client wants to observe.
 */
@NullUnmarked // Null-correctness is similar to that of ObservableSupplierImpl.
class TransitiveObservableSupplier<
                ParentT, ChildT, FuncT extends @Nullable NullableObservableSupplier<ChildT>>
        extends ObservableSupplierImpl<ChildT> {
    private final Callback<ParentT> mOnParentSupplierChangeCallback = this::onParentSupplierChange;
    private final Callback<ChildT> mOnTargetSupplierChangeCallback = this::set;
    private final NullableObservableSupplier<ParentT> mParentSupplier;
    private final Function<ParentT, FuncT> mUnwrapFunction;

    // When this is set, then mOnTargetSupplierChangeCallback is an observer of the object
    // referenced by mCurrentTargetSupplier. When this value is changed, the observer must be
    // removed.
    private @Nullable NullableObservableSupplier<ChildT> mCurrentTargetSupplier;
    private final @Nullable ChildT mDefaultValue;

    // Create using ObservableSuppliers.createTransitive().
    TransitiveObservableSupplier(
            NullableObservableSupplier<ParentT> parentSupplier,
            Function<ParentT, FuncT> unwrapFunction,
            ChildT defaultValue,
            boolean allowSetToNull) {
        super(defaultValue, allowSetToNull);
        mParentSupplier = parentSupplier;
        mUnwrapFunction = unwrapFunction;
        mDefaultValue = defaultValue;
        assertCompatibleSupplier(parentSupplier);
    }

    @Override
    public ChildT addObserver(Callback<ChildT> obs, @NotifyBehavior int behavior) {
        if (!super.hasObservers()) {
            onParentSupplierChange(
                    mParentSupplier.addSyncObserver(mOnParentSupplierChangeCallback));
        }
        return super.addObserver(obs, behavior);
    }

    @Override
    public void removeObserver(Callback<ChildT> obs) {
        super.removeObserver(obs);
        if (!super.hasObservers()) {
            mParentSupplier.removeObserver(mOnParentSupplierChangeCallback);
            if (mCurrentTargetSupplier != null) {
                mCurrentTargetSupplier.removeObserver(mOnTargetSupplierChangeCallback);
                mCurrentTargetSupplier = null;
            }
        }
    }

    @NullUnmarked // Needs to work where ChildT is non-null or nullable.
    @Override
    public ChildT get() {
        // If we have no observers, our copy of the value is not kept current.
        // Also - do not call any delegates after destroy(), since it might not be safe to do so.
        if (mObservers != null && mObservers.isEmpty()) {
            ChildT ret = null;
            ParentT parentValue = mParentSupplier.get();
            if (parentValue != null) {
                NullableObservableSupplier<ChildT> targetSupplier =
                        mUnwrapFunction.apply(parentValue);
                if (targetSupplier != null) {
                    assertCompatibleSupplier(targetSupplier);
                    ret = targetSupplier.get();
                }
            }
            if (ret == null) {
                ret = mDefaultValue;
            }
            // Call set to run null check.
            if (ret != mObject) {
                set(ret);
            }
        }
        // Call super.get() for thread checks.
        return super.get();
    }

    private void assertCompatibleSupplier(NullableObservableSupplier<?> other) {
        // Ensure that if we are non-null or monotonic, that the transitive supplier is non-null or
        // monotonic.
        assert mDefaultValue != null
                        || !Boolean.FALSE.equals(mAllowSetToNull)
                        || !Boolean.TRUE.equals(BaseObservableSupplierImpl.allowsSetToNull(other))
                : "Root supplier set as non-null, but the transitive one is not.";
    }

    /**
     * Conceptually this just removes our observer from the old target supplier, and our observer to
     * to the new target supplier. In practice this is full of null checks. We also have to make
     * sure we keep our delegate supplier's value up to date, which is also what drives client
     * observations.
     */
    @NullUnmarked // Needs to work where ChildT is non-null or nullable.
    private void onParentSupplierChange(ParentT parentValue) {
        if (mCurrentTargetSupplier != null) {
            mCurrentTargetSupplier.removeObserver(mOnTargetSupplierChangeCallback);
        }

        // Keep track of the current target supplier, because if this ever changes, we'll need to
        // remove our observer from it.
        mCurrentTargetSupplier = parentValue == null ? null : mUnwrapFunction.apply(parentValue);

        ChildT targetValue = null;
        if (mCurrentTargetSupplier != null) {
            assertCompatibleSupplier(mCurrentTargetSupplier);

            // While addObserver will call us if a value is already set, we do not want to depend on
            // that for two reasons. If there is no value set, we need to null out our supplier now.
            // And if there is a value set, we're going to get invoked asynchronously, which means
            // our delegate supplier could be in an intermediately incorrect state. By just setting
            // our delegate eagerly we avoid both problems.
            targetValue = mCurrentTargetSupplier.addSyncObserver(mOnTargetSupplierChangeCallback);
        }
        if (targetValue == null) {
            targetValue = mDefaultValue;
        }
        if (targetValue != mObject) {
            set(targetValue);
        }
    }
}
