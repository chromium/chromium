// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.supplier;

import androidx.annotation.IntDef;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

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
 *
 * @param <E> The type of the wrapped object.
 */
@NullMarked
public interface ObservableSupplier<E> extends Supplier<E> {
    /**
     * A bitmask of flags that control the notification behavior of {@link #addObserver(Callback,
     * int)}.
     */
    @IntDef(
            flag = true,
            value = {
                NotifyBehavior.NOTIFY_ON_ADD,
                NotifyBehavior.POST_ON_ADD,
                NotifyBehavior.NONE,
            })
    @interface NotifyBehavior {

        /**
         * If set, the observer in {@link #addObserver(Callback, int)} method will be called upon
         * being added.
         */
        int NOTIFY_ON_ADD = 1 << 0;

        /**
         * If set, the observer in {@link #addObserver(Callback, int)} method will be called
         * asynchronously upon being added. This flag only has an effect if {@link #NOTIFY_ON_ADD}
         * is set.
         */
        int POST_ON_ADD = 1 << 1;

        /** All flags disabled. */
        int NONE = 0;
    }

    /**
     * Adds an observer, which is then notified upon the dependency being modified. Additional
     * behaviors are specified through |behavior|.
     *
     * @param obs An observer to be added.
     * @param behavior A bitmask of {@link NotifyBehavior} flags that control the notification
     *     behavior.
     * @return The current object or null if it hasn't been set yet.
     */
    @Nullable E addObserver(Callback<E> obs, @NotifyBehavior int behavior);

    /**
     * Adds a synchronous observer. Equivalent to calling {@link #addObserver(Callback, int)} with
     * {@link NotifyBehavior#NONE}.
     *
     * @param obs The observer to add.
     * @return The current object if available, otherwise null.
     */
    default @Nullable E addSyncObserver(Callback<E> obs) {
        return addObserver(obs, NotifyBehavior.NONE);
    }

    /**
     * Adds an observer that is called immediately, if the value is non-null.
     *
     * <p>Equivalent to calling {@link #addObserver(Callback, int)} with {@link
     * NotifyBehavior#NOTIFY_ON_ADD}.
     *
     * @param obs The observer to add.
     * @return The current object if available, otherwise null.
     */
    default @Nullable E addSyncObserverAndCallIfNonNull(Callback<E> obs) {
        return addObserver(obs, NotifyBehavior.NOTIFY_ON_ADD);
    }

    /**
     * Adds an observer that is asynchronously called immediately, if the value is non-null.
     *
     * <p>Equivalent to calling {@link #addObserver(Callback, int)} with {@link
     * NotifyBehavior#NOTIFY_ON_ADD} | {@link NotifyBehavior#POST_ON_ADD}.
     *
     * @param obs The observer to add.
     * @return The current object if available, otherwise null.
     */
    default @Nullable E addSyncObserverAndPostIfNonNull(Callback<E> obs) {
        return addObserver(obs, NotifyBehavior.NOTIFY_ON_ADD | NotifyBehavior.POST_ON_ADD);
    }

    /**
     * Adds a synchronous observer, which will be notified when the object owned by this supplier is
     * available. If the object is already available, the callback will be notified asynchronously
     * at the end of the current message loop (so long as the object hasn't changed).
     *
     * <p>Equivalent to calling {@link #addObserver(Callback, int)} with {@link
     * NotifyBehavior#NOTIFY_ON_ADD} | {@link NotifyBehavior#POST_ON_ADD}.
     *
     * @param obs An observer to be added.
     * @return The current object or null if it hasn't been set yet.
     */
    default @Nullable E addObserver(Callback<E> obs) {
        return addSyncObserverAndPostIfNonNull(obs);
    }

    /**
     * @param obs The observer to remove.
     */
    void removeObserver(Callback<E> obs);
}
