// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.supplier;

import com.google.errorprone.annotations.DoNotMock;

import org.chromium.base.Callback;
import org.chromium.base.supplier.MonotonicObservableSupplier.NotifyBehavior;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.function.Function;
import java.util.function.Supplier;

/** An interface for Suppliers that can be observed. Implementations are not thread-safe. */
@NullMarked
// The mix-in methods here make tests brittle when mocked. We also do not want tests to simulate
// callbacks incorrectly (e.g. calling them synchronously when they should be posted).
@DoNotMock("Mock the thing you are supplying instead.")
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
     * <ul>
     *   <li>Posted callbacks are not run if removeObserver() is called before they are run.
     *   <li>Posted callbacks are not run if set() is called with a new value before they are run.
     * </ul>
     *
     * @param obs The observer to add.
     * @return The current value of the supplier.
     */
    default @Nullable T addSyncObserverAndPostIfNonNull(Callback<@Nullable T> obs) {
        return addObserver(obs, NotifyBehavior.NOTIFY_ON_ADD | NotifyBehavior.POST_ON_ADD);
    }

    /** Returns whether there are any observers. */
    default boolean hasObservers() {
        return getObserverCount() != 0;
    }

    /** Returns the number of observers. */
    int getObserverCount();

    /**
     * Creates an ObservableSupplier that tracks an ObservableSupplier of this ObservableSupplier.
     */
    @SuppressWarnings("Unchecked")
    default <
                    ChildT,
                    FuncT extends @Nullable T,
                    FuncSup extends @Nullable NullableObservableSupplier<ChildT>>
            SettableNullableObservableSupplier<ChildT> createTransitiveNullable(
                    Function<FuncT, FuncSup> unwrapFunction) {
        return new TransitiveObservableSupplier<>(
                (NullableObservableSupplier) this,
                unwrapFunction,
                /* defaultValue= */ null,
                /* allowSetToNull= */ true);
    }

    /**
     * Creates an ObservableSupplier that tracks an ObservableSupplier of this ObservableSupplier.
     * If either supplier has not yet been initialized, uses the given default value. The current
     * and transitive suppliers must both be non-null or monotonic.
     */
    @SuppressWarnings("Unchecked")
    default <ChildT> SettableNonNullObservableSupplier<ChildT> createTransitiveNonNull(
            ChildT defaultValue, Function<T, NonNullObservableSupplier<ChildT>> unwrapFunction) {
        return new TransitiveObservableSupplier<>(
                (NullableObservableSupplier) this,
                unwrapFunction,
                defaultValue,
                /* allowSetToNull= */ false);
    }

    /** Creates an ObservableSupplier that tracks a value derived from this ObservableSupplier. */
    @SuppressWarnings("Unchecked")
    default <ChildT, FuncT extends @Nullable T>
            SettableNullableObservableSupplier<ChildT> createDerivedNullable(
                    Function<FuncT, @Nullable ChildT> unwrapFunction) {
        return new UnwrapObservableSupplier<>(
                (NullableObservableSupplier) this, unwrapFunction, /* allowSetToNull= */ true);
    }

    /** Creates an ObservableSupplier that tracks a value derived from this ObservableSupplier. */
    @SuppressWarnings("Unchecked")
    default <ChildT, FuncT extends @Nullable T>
            SettableNonNullObservableSupplier<ChildT> createDerivedNonNull(
                    Function<FuncT, ChildT> unwrapFunction) {
        return new UnwrapObservableSupplier<>(
                        (NullableObservableSupplier) this,
                        unwrapFunction,
                        /* allowSetToNull= */ false)
                .asNonNull();
    }
}
