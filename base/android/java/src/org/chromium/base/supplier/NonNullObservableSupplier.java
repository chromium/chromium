// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.supplier;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;

import java.util.function.Supplier;

/**
 * A specialization of {@link ObservableSupplier} that is guaranteed to not supply null.
 *
 * @param <T> The type of the result.
 */
@NullMarked
// TODO(agrieve): Add @DoNotMock
public interface NonNullObservableSupplier<T> extends Supplier<T>, ObservableSupplier<T> {
    @Override
    default T addSyncObserver(Callback<T> obs) {
        return addObserver(obs, ObservableSupplier.NotifyBehavior.NONE);
    }

    @Override
    default T addSyncObserverAndCallIfNonNull(Callback<T> obs) {
        return addObserver(obs, ObservableSupplier.NotifyBehavior.NOTIFY_ON_ADD);
    }

    @Override
    default T addSyncObserverAndPostIfNonNull(Callback<T> obs) {
        return addObserver(
                obs,
                ObservableSupplier.NotifyBehavior.NOTIFY_ON_ADD
                        | ObservableSupplier.NotifyBehavior.POST_ON_ADD);
    }

    @Override
    default T addObserver(Callback<T> obs) {
        return addSyncObserverAndPostIfNonNull(obs);
    }

    @Override
    T addObserver(Callback<T> obs, @NotifyBehavior int behavior);
}
