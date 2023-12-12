// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.supplier.ObservableSupplier;

import java.util.Objects;

/**
 * A callback adapter that caches the last value and supplies both the last and current result to
 * the {@link ValueChangedObserver}.
 *
 * <p>This is useful where cleanup needs to be done using the old value before switching to the new
 * value. For example, unregistering observers from the old value and registering them on a new
 * value. This is particularly useful {@link ObservableSupplier}.
 *
 * @param <T> The type to observe.
 */
public class ValueChangedCallback<T> implements Callback<T> {
    /**
     * Interface for observers that care about monitoring both the old and new values when a
     * callback is invoked.
     *
     * @param <T> The type to observe.
     */
    @FunctionalInterface
    public interface ValueChangedObserver<T> {
        /**
         * Called when the {@link Callback} is invoked with both new and old values.
         *
         * @param newValue The new value.
         * @param oldValue The previous value. Depending on what is being observed this might not be
         *     valid to use anymore.
         */
        public void onValueChanged(@Nullable T newValue, @Nullable T oldValue);
    }

    private final @NonNull ValueChangedObserver<T> mValueChangedObserver;
    private @Nullable T mLastValue;

    /**
     * @param onValueChangedObserver The {@link ValueChangedObserver} that receives updates.
     */
    public ValueChangedCallback(@NonNull ValueChangedObserver<T> onValueChangedObserver) {
        mValueChangedObserver = onValueChangedObserver;
    }

    @Override
    public void onResult(T newValue) {
        if (Objects.equals(newValue, mLastValue)) return;
        T oldLastValue = mLastValue;
        mLastValue = newValue;
        mValueChangedObserver.onValueChanged(newValue, oldLastValue);
    }
}
