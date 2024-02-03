// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.supplier;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;

import java.util.function.Function;

/**
 * A simplified version of {@link org.chromium.base.supplier.TransitiveObservableSupplier} that only
 * has one {@link ObservableSupplier}, but still needs unwrapping to get at the target value. Care
 * should be taken to make sure null is handled in unwrap.
 *
 * <p> For example, suppose we have an ObservableSupplier giving us a Car object. Car objects
 * contain an Engine object. We're going to construct a helper class that really wants to operate on
 * an Engine. Giving it a Car object doesn't really make sense, it shouldn't be aware of what a Car
 * is. Our usage could look something like:
 *
 * <pre> {@code
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
 * @param <P> The parent object that's holding the target value somehow.
 * @param <T> The target type that the client wants to observe.
 */
public class UnwrapObservableSupplier<P, T> implements ObservableSupplier<T> {
    private final @NonNull ObservableSupplierImpl<T> mDelegateSupplier =
            new ObservableSupplierImpl<>();
    private final @NonNull Callback<P> mOnParentSupplierChangeCallback =
            this::onParentSupplierChange;
    private final @NonNull ObservableSupplier<P> mParentSupplier;
    private final @NonNull Function<P, T> mUnwrapFunction;

    /**
     * @param parentSupplier The parent observable supplier.
     * @param unwrapFunction Converts the parent value to target value. Should handle null values.
     */
    public UnwrapObservableSupplier(
            @NonNull ObservableSupplier<P> parentSupplier, @NonNull Function<P, T> unwrapFunction) {
        mParentSupplier = parentSupplier;
        mUnwrapFunction = unwrapFunction;
    }

    @Override
    public T get() {
        return mUnwrapFunction.apply(mParentSupplier.get());
    }

    @Override
    public T addObserver(Callback<T> obs) {
        // Can use mDelegateSupplier.hasObservers() to tell if we are subscribed or not to
        // mParentSupplier. This is safe because we never expose outside callers, and completely
        // control when we add/remove observers to it.
        if (!mDelegateSupplier.hasObservers()) {
            // The value in mDelegateSupplier is stale or has never been set, and so we update it
            // by passing through the current parent value to our on change method.
            onParentSupplierChange(mParentSupplier.addObserver(mOnParentSupplierChangeCallback));
        }
        return mDelegateSupplier.addObserver(obs);
    }

    @Override
    public void removeObserver(Callback<T> obs) {
        mDelegateSupplier.removeObserver(obs);
        // If no one is observing, we do not need to observe parent. This allows our callers to
        // destroy themselves, unsubscribe from us, and we remove our observer from the parent,
        // fully cleaning everything up.
        if (!mDelegateSupplier.hasObservers()) {
            mParentSupplier.removeObserver(mOnParentSupplierChangeCallback);
        }
    }

    private void onParentSupplierChange(@Nullable P parentValue) {
        mDelegateSupplier.set(mUnwrapFunction.apply(parentValue));
    }
}
