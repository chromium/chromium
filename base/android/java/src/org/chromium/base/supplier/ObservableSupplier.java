// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.supplier;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.function.Function;
import java.util.function.Supplier;

/**
 * ObservableSupplier wraps an asynchronously provided object E, notifying observers when the
 * dependency is available. This allows classes dependent on E to be provided with a
 * ObservableSupplier during construction and register a Callback<E> to be notified when the needed
 * dependency is available.
 *
 * <p>This class must only be accessed from a single thread.
 *
 * <p>For classes owning the ObservableSupplier and providing it as a dependency to others, see
 * {@link ObservableSupplierImpl}.
 *
 * <p>The behavior of the observer is different depending on which method is called.
 */
@NullMarked
// TODO(455874046): Supplier<T> -> Supplier<@Nullable T>
@SuppressWarnings("NullAway") // Remove "T extends @Nullable Object"
public interface ObservableSupplier<T extends @Nullable Object>
        extends Supplier<T>, NullableObservableSupplier<T> {

    /** Defines the behavior of the notification when an observer is added. */
    @interface NotifyBehavior {
        int NONE = 0;
        int NOTIFY_ON_ADD = 1;
        int POST_ON_ADD = 1 << 1;
    }

    @SuppressWarnings("NullAway") // Changing nullness of Callback<T>
    @Override
    @Nullable T addObserver(Callback<T> obs, @NotifyBehavior int behavior);

    @SuppressWarnings("NullAway") // Changing nullness of Callback<T>
    @Override
    void removeObserver(Callback<T> obs);

    @SuppressWarnings("NullAway") // Changing nullness of Callback<T>
    @Override
    default @Nullable T addSyncObserver(Callback<T> obs) {
        return addObserver(obs, NotifyBehavior.NONE);
    }

    @SuppressWarnings("NullAway") // Changing nullness of Callback<T>
    @Override
    default @Nullable T addSyncObserverAndCallIfNonNull(Callback<T> obs) {
        return addObserver(obs, NotifyBehavior.NOTIFY_ON_ADD);
    }

    @SuppressWarnings("NullAway") // Changing nullness of Callback<T>
    @Override
    default @Nullable T addSyncObserverAndPostIfNonNull(Callback<T> obs) {
        return addObserver(obs, NotifyBehavior.NOTIFY_ON_ADD | NotifyBehavior.POST_ON_ADD);
    }

    @SuppressWarnings("NullAway") // Changing nullness of Callback<T>
    @Override
    default @Nullable T addObserver(Callback<T> obs) {
        return addSyncObserverAndPostIfNonNull(obs);
    }

    /**
     * @return A {@link NonNullObservableSupplier} if the supplied value is not null.
     */
    @SuppressWarnings("Unchecked")
    default NonNullObservableSupplier<T> asNonNull() {
        // Cast from monotonic non-null -> non-null.
        assert !Boolean.TRUE.equals(BaseObservableSupplierImpl.allowsSetToNull(this))
                : "Cannot cast a non-monotonic supplier to a NonNull one.";
        assert get() != null : "Supplier is monotonic, but does not yet have a value.";
        return (NonNullObservableSupplier<T>) this;
    }

    /**
     * Creates an ObservableSupplier that tracks an ObservableSupplier of this ObservableSupplier.
     */
    @SuppressWarnings("Unchecked")
    default <ChildT, FuncT extends ObservableSupplier<ChildT>>
            SettableObservableSupplier<ChildT> createTransitiveMonotonic(
                    Function<T, FuncT> unwrapFunction) {
        return new TransitiveObservableSupplier<>(
                this,
                (Function) unwrapFunction,
                /* defaultValue= */ null,
                /* allowSetToNull= */ false);
    }

    /**
     * Creates an ObservableSupplier that tracks an ObservableSupplier of this ObservableSupplier.
     * The current and transitive suppliers must both be non-null or monotonic.
     */
    @SuppressWarnings("Unchecked")
    default <ChildT> SettableNonNullObservableSupplier<ChildT> createTransitiveNonNull(
            Function<T, NonNullObservableSupplier<ChildT>> unwrapFunction) {
        // asNonNull() will call get(), which will update the initial value to be non-null.
        return new TransitiveObservableSupplier<>(
                        (NullableObservableSupplier) this,
                        unwrapFunction,
                        /* defaultValue= */ null,
                        /* allowSetToNull= */ false)
                .asNonNull();
    }
}
