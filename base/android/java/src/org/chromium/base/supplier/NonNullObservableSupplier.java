// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.supplier;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;

import java.util.function.Supplier;

/**
 * A specialization of {@link MonotonicObservableSupplier} that is guaranteed to not supply null.
 *
 * @param <T> The type of the result.
 */
@NullMarked
// TODO(agrieve): Add @DoNotMock
public interface NonNullObservableSupplier<T> extends Supplier<T>, MonotonicObservableSupplier<T> {
    @Override
    default T addSyncObserver(Callback<T> obs) {
        return addObserver(obs, MonotonicObservableSupplier.NotifyBehavior.NONE);
    }

    @Override
    default T addSyncObserverAndCallIfNonNull(Callback<T> obs) {
        return addObserver(obs, MonotonicObservableSupplier.NotifyBehavior.NOTIFY_ON_ADD);
    }

    @Override
    default T addSyncObserverAndPostIfNonNull(Callback<T> obs) {
        return addObserver(
                obs,
                MonotonicObservableSupplier.NotifyBehavior.NOTIFY_ON_ADD
                        | MonotonicObservableSupplier.NotifyBehavior.POST_ON_ADD);
    }

    @Override
    default T addObserver(Callback<T> obs) {
        return addSyncObserverAndPostIfNonNull(obs);
    }

    @Override
    T addObserver(Callback<T> obs, @NotifyBehavior int behavior);
}
