// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.chromium.base.supplier.ObservableSuppliers.createNullable;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier.NotifyBehavior;
import org.chromium.base.supplier.SettableNullableObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.Objects;

/**
 * An observable supplier that allows for the registration of observers that are notified of the new
 * value before the value of the supplier is actually changed.
 */
@NullMarked
public class LookAheadObservableSupplier<T> implements SettableNullableObservableSupplier<T> {
    private final SettableNullableObservableSupplier<T> mLookAheadSupplier = createNullable();
    private final SettableNullableObservableSupplier<T> mObservableSupplier = createNullable();
    private boolean mLookAheadValueSet;

    @Override
    public void set(@Nullable T value) {
        if (!mLookAheadValueSet) {
            mLookAheadSupplier.set(value);
        } else {
            assert Objects.equals(value, mLookAheadSupplier.get());
        }

        mObservableSupplier.set(value);
        mLookAheadValueSet = false;
    }

    @Override
    public void destroy() {
        mObservableSupplier.destroy();
        mLookAheadSupplier.destroy();
    }

    /**
     * Manually declare to look ahead observers that the supplier will be set to |value|.
     *
     * @param value The value the supplier will be set to.
     */
    public void willSet(@Nullable T value) {
        mLookAheadSupplier.set(value);
        mLookAheadValueSet = true;
    }

    /**
     * Adds an observer that is notified before the supplier's value is changed. The observer
     * receives the new value that is about to be set.
     *
     * @param obs The observer to add.
     */
    public @Nullable T addLookAheadObserver(Callback<@Nullable T> obs) {
        return mLookAheadSupplier.addObserver(obs);
    }

    /**
     * Adds an observer that is notified before the supplier's value is changed. The observer
     * receives the new value that is about to be set.
     *
     * @param obs The observer to add.
     * @param behavior The {@link NotifyBehavior} the observer will exhibit.
     */
    public @Nullable T addLookAheadObserver(
            Callback<@Nullable T> obs, @NotifyBehavior int behavior) {
        return mLookAheadSupplier.addObserver(obs, behavior);
    }

    /**
     * Removes a look ahead observer.
     *
     * @param obs The observer to remove.
     */
    public void removeLookAheadObserver(Callback<@Nullable T> obs) {
        mLookAheadSupplier.removeObserver(obs);
    }

    @Override
    public @Nullable T addObserver(Callback<@Nullable T> obs, int behavior) {
        return mObservableSupplier.addObserver(obs, behavior);
    }

    @Override
    public void removeObserver(Callback<@Nullable T> obs) {
        mObservableSupplier.removeObserver(obs);
    }

    @Override
    public int getObserverCount() {
        return mObservableSupplier.getObserverCount() + mLookAheadSupplier.getObserverCount();
    }

    @Override
    public T get() {
        return mObservableSupplier.get();
    }
}
