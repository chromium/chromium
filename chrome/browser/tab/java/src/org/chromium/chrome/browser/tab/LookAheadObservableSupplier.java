// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import org.chromium.base.Callback;
import org.chromium.base.supplier.MonotonicObservableSupplier.NotifyBehavior;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * An observable supplier that allows for the registration of observers that are notified of the new
 * value before the value of the supplier is actually changed.
 */
@NullMarked
public interface LookAheadObservableSupplier<T> extends NullableObservableSupplier<T> {
    /**
     * Adds an observer that is notified before the supplier's value is changed. The observer
     * receives the new value that is about to be set.
     *
     * @param obs The observer to add.
     */
    @Nullable T addLookAheadObserver(Callback<@Nullable T> obs);

    /**
     * Adds an observer that is notified before the supplier's value is changed. The observer
     * receives the new value that is about to be set.
     *
     * @param obs The observer to add.
     * @param behavior The {@link NotifyBehavior} the observer will exhibit.
     */
    @Nullable T addLookAheadObserver(Callback<@Nullable T> obs, @NotifyBehavior int behavior);

    /**
     * Removes a look ahead observer.
     *
     * @param obs The observer to remove.
     */
    void removeLookAheadObserver(Callback<@Nullable T> obs);
}
