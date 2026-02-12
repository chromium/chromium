// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.chromium.base.supplier.ObservableSuppliers.createNullable;

import org.chromium.base.Callback;
import org.chromium.base.supplier.MonotonicObservableSupplier.NotifyBehavior;
import org.chromium.base.supplier.SettableNullableObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.LookAheadObservableSupplier;

import java.util.Objects;

/** An settable implementation of {@link LookAheadObservableSupplier}. */
@NullMarked
public class SettableLookAheadObservableSupplier<T>
        implements LookAheadObservableSupplier<T>, SettableNullableObservableSupplier<T> {
    private final SettableNullableObservableSupplier<T> mLookAheadSupplier = createNullable();
    private final SettableNullableObservableSupplier<T> mObservableSupplier = createNullable();
    private boolean mLookAheadValueSet;

    /**
     * Manually declare to look ahead observers that the supplier will be set to |value|.
     *
     * @param value The value the supplier will be set to.
     */
    public void willSet(@Nullable T value) {
        mLookAheadSupplier.set(value);
        mLookAheadValueSet = true;
    }

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

    @Override
    public @Nullable T addLookAheadObserver(Callback<@Nullable T> obs) {
        return mLookAheadSupplier.addSyncObserverAndPostIfNonNull(obs);
    }

    @Override
    public @Nullable T addLookAheadObserver(
            Callback<@Nullable T> obs, @NotifyBehavior int behavior) {
        return mLookAheadSupplier.addObserver(obs, behavior);
    }

    @Override
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
    public @Nullable T get() {
        return mObservableSupplier.get();
    }
}
