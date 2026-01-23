// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.supplier;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Factory methods for ObservableSuppliers. */
@NullMarked
public class ObservableSuppliers {
    private static final MonotonicObservableSupplier<?> ALWAYS_NULL =
            new BaseObservableSupplierImpl<>(/* allowSetToNull= */ false);

    /** Creates an ObservableSupplier that can be set to null. */
    public static <T> SettableNullableObservableSupplier<T> createNullable() {
        return new ObservableSupplierImpl<T>(/* initialValue= */ null, /* allowSetToNull= */ true);
    }

    /** Creates an ObservableSupplier that can be set to null. */
    public static <T> SettableNullableObservableSupplier<T> createNullable(
            @Nullable T initialValue) {
        return new ObservableSupplierImpl<T>(initialValue, /* allowSetToNull= */ true);
    }

    /**
     * Creates an ObservableSupplier that can start null, but not be set to null (enforced via an
     * assert).
     */
    public static <T> SettableMonotonicObservableSupplier<T> createMonotonic() {
        return new ObservableSupplierImpl<T>(/* initialValue= */ null, /* allowSetToNull= */ false);
    }

    /**
     * Creates an ObservableSupplier that can start null, but not be set to null (enforced via an
     * assert).
     */
    public static <T> SettableMonotonicObservableSupplier<T> createMonotonic(
            @Nullable T initialValue) {
        return new ObservableSupplierImpl<T>(
                /* initialValue= */ initialValue, /* allowSetToNull= */ false);
    }

    /** Creates an NonNullObservableSupplier that can never be null. */
    public static <T> SettableNonNullObservableSupplier<T> createNonNull(T initialValue) {
        return new ObservableSupplierImpl<T>(initialValue, /* allowSetToNull= */ false);
    }

    // Convenience methods. These cannot be changed to share an instance (like alwaysNull), because
    // addObserver() posts a callback that must be cancelled if removeObserver() is called before it
    // gets schedules.
    public static NonNullObservableSupplier<Boolean> alwaysTrue() {
        return createNonNull(true);
    }

    public static NonNullObservableSupplier<Boolean> alwaysFalse() {
        return createNonNull(false);
    }

    public static NonNullObservableSupplier<Integer> alwaysZero() {
        return createNonNull(0);
    }

    public static <T> MonotonicObservableSupplier<T> alwaysNull() {
        return (MonotonicObservableSupplier<T>) ALWAYS_NULL;
    }

    public static <T> NonNullObservableSupplier<T> of(T value) {
        return createNonNull(value);
    }

    private ObservableSuppliers() {}
}
