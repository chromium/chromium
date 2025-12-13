// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.supplier;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullUnmarked;

import java.util.function.Function;

/**
 * A simplified version of {@link org.chromium.base.supplier.TransitiveObservableSupplier} that only
 * has one {@link ObservableSupplier}, but still needs unwrapping to get at the target value. Care
 * should be taken to make sure null is handled in unwrap.
 *
 * <ParentT> For example, suppose we have an ObservableSupplier giving us a Car object. Car objects
 * contain an Engine object. We're going to construct a helper class that really wants to operate on
 * an Engine. Giving it a Car object doesn't really make sense, it shouldn't be aware of what a Car
 * is. Our usage could look something like:
 *
 * <ParentTre> {@code
 * public class Mechanic {
 *     private static class EngineTester {
 *         EngineTester(ObservableSupplier<Engine> engineSupplier) {}
 *     }
 *
 *     private final EngineTester mEngineTester;
 *
 *     public Mechanic(ObservableSupplier<Car> input) {
 *         mEngineTester =
 *                 new EngineTester(new TransitiveObservableSupplier<Car>(input,
 *                     this::unwrapEngine));
 *     }
 *
 *     private Engine unwrapEngine(@Nullable Car car) {
 *         return car == null ? null : car.getEngine();
 *     }
 * } </pre>
 *
 * @param <ParentT> The parent object that's holding the target value somehow.
 * @param <ChildT> The target type that the client wants to observe.
 */
@NullUnmarked // Null-correctness is similar to that of ObservableSupplierImpl.
class UnwrapObservableSupplier<ParentT, ChildT> extends ObservableSupplierImpl<ChildT> {
    private final Callback<ParentT> mOnParentSupplierChangeCallback = this::onParentSupplierChange;
    private final NullableObservableSupplier<ParentT> mParentSupplier;
    private final Function<ParentT, ChildT> mUnwrapFunction;

    /**
     * @param parentSupplier The parent observable supplier.
     * @param unwrapFunction Converts the parent value to target value. Should handle null values.
     */
    UnwrapObservableSupplier(
            NullableObservableSupplier<ParentT> parentSupplier,
            Function<ParentT, ChildT> unwrapFunction,
            boolean allowSetToNull) {
        super(null, allowSetToNull);
        mParentSupplier = parentSupplier;
        mUnwrapFunction = unwrapFunction;
    }

    @Override
    public ChildT get() {
        return mUnwrapFunction.apply(mParentSupplier.get());
    }

    @Override
    public ChildT addObserver(Callback<ChildT> obs, @NotifyBehavior int behavior) {
        // Can use hasObservers() to tell if we are subscribed or not to
        // mParentSupplier. This is safe because we never expose outside callers, and completely
        // control when we add/remove observers to it.
        if (!hasObservers()) {
            // The value in mDelegateSupplier is stale or has never been set, and so we update it
            // by passing through the current parent value to our on change method.
            mParentSupplier.addSyncObserverAndCallIfNonNull(mOnParentSupplierChangeCallback);
        }
        return super.addObserver(obs, behavior);
    }

    @Override
    public void removeObserver(Callback<ChildT> obs) {
        super.removeObserver(obs);
        // If no one is observing, we do not need to observe parent. This allows our callers to
        // destroy themselves, unsubscribe from us, and we remove our observer from the parent,
        // fully cleaning everything up.
        if (!super.hasObservers()) {
            mParentSupplier.removeObserver(mOnParentSupplierChangeCallback);
        }
    }

    private void onParentSupplierChange(ParentT parentValue) {
        set(mUnwrapFunction.apply(parentValue));
    }
}
