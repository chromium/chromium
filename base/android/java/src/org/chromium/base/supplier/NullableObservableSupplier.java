// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.supplier;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier.NotifyBehavior;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.function.Function;
import java.util.function.Supplier;

/** An interface for classes that can be observed. */
@NullMarked
public interface NullableObservableSupplier<T> extends Supplier<@Nullable T> {
    /**
     * Adds an observer to the supplier.
     *
     * @param obs The observer to add.
     * @param behavior The notification behavior.
     * @return The current value of the supplier.
     */
    @Nullable T addObserver(Callback<@Nullable T> obs, @NotifyBehavior int behavior);

    /** Removes the given observer. */
    void removeObserver(Callback<@Nullable T> obs);

    /**
     * Adds a synchronous observer to the supplier.
     *
     * @param obs The observer to add.
     * @return The current value of the supplier.
     */
    default @Nullable T addSyncObserver(Callback<@Nullable T> obs) {
        return addObserver(obs, NotifyBehavior.NONE);
    }

    /**
     * Adds a synchronous observer to the supplier and calls it if the value is not null.
     *
     * @param obs The observer to add.
     * @return The current value of the supplier.
     */
    default @Nullable T addSyncObserverAndCallIfNonNull(Callback<@Nullable T> obs) {
        return addObserver(obs, NotifyBehavior.NOTIFY_ON_ADD);
    }

    /**
     * Adds a synchronous observer to the supplier and posts a notification if the value is not
     * null.
     *
     * @param obs The observer to add.
     * @return The current value of the supplier.
     */
    default @Nullable T addSyncObserverAndPostIfNonNull(Callback<@Nullable T> obs) {
        return addObserver(obs, NotifyBehavior.NOTIFY_ON_ADD | NotifyBehavior.POST_ON_ADD);
    }

    /**
     * Adds an observer to the supplier and posts a notification if the value is not null.
     *
     * @param obs The observer to add.
     * @return The current value of the supplier.
     */
    default @Nullable T addObserver(Callback<@Nullable T> obs) {
        return addSyncObserverAndPostIfNonNull(obs);
    }

    /** Returns whether there are any observers. */
    boolean hasObservers();

    /**
     * Creates an ObservableSupplier that tracks an ObservableSupplier of this ObservableSupplier.
     */
    @SuppressWarnings("Unchecked")
    default <
                    ChildT,
                    FuncT extends @Nullable T,
                    FuncSup extends @Nullable NullableObservableSupplier<ChildT>>
            NullableObservableSupplier<ChildT> createTransitiveNullable(
                    Function<FuncT, FuncSup> unwrapFunction) {
        return new TransitiveObservableSupplier<>(
                (NullableObservableSupplier) this,
                unwrapFunction,
                /* initialValue= */ null,
                /* allowSetToNull= */ true);
    }

    /** Creates an ObservableSupplier that tracks a value derived from this ObservableSupplier. */
    @SuppressWarnings("Unchecked")
    default <ChildT, FuncT extends @Nullable T>
            NullableObservableSupplier<ChildT> createDerivedNullable(
                    Function<FuncT, @Nullable ChildT> unwrapFunction) {
        return new UnwrapObservableSupplier<>(
                (NullableObservableSupplier) this, unwrapFunction, /* allowSetToNull= */ true);
    }

    /** Creates an ObservableSupplier that tracks a value derived from this ObservableSupplier. */
    @SuppressWarnings("Unchecked")
    default <ChildT, FuncT extends @Nullable T>
            NonNullObservableSupplier<ChildT> createDerivedNonNull(
                    Function<FuncT, ChildT> unwrapFunction) {
        return new UnwrapObservableSupplier<>(
                        (NullableObservableSupplier) this,
                        unwrapFunction,
                        /* allowSetToNull= */ false)
                .asNonNull();
    }
}
