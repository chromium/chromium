// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.supplier;

import static org.chromium.build.NullUtil.assertNonNull;
import static org.chromium.build.NullUtil.assumeNonNull;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.build.BuildConfig;
import org.chromium.build.annotations.Contract;
import org.chromium.build.annotations.NonNull;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.function.Supplier;

/** Utilities for interactions with Suppliers. */
@NullMarked
public class SupplierUtils {
    @SuppressWarnings("NullAway") // Might be fixed by https://github.com/uber/NullAway/issues/1455
    private static final Supplier<?> NULL_SUPPLIER = () -> null;

    private SupplierUtils() {}

    private static class Barrier {
        private int mWaitingCount;
        private @Nullable Runnable mCallback;

        void waitForAll(Runnable callback, Supplier<?>... suppliers) {
            ThreadUtils.assertOnUiThread();
            assert mCallback == null;
            int waitingSupplierCount = 0;
            Callback<?> supplierCallback = (unused) -> onSupplierAvailable();
            for (Supplier<?> supplier : suppliers) {
                if (supplier.get() != null) {
                    continue;
                }

                waitingSupplierCount++;
                if (supplier instanceof NullableObservableSupplier<?>) {
                    NullableObservableSupplier<?> observableSupplier =
                            ((NullableObservableSupplier<?>) supplier);
                    new OneShotCallback(observableSupplier, supplierCallback);
                } else if (supplier instanceof OneshotSupplier) {
                    ((OneshotSupplier) supplier).onAvailable(supplierCallback);
                } else if (supplier instanceof SyncOneshotSupplier) {
                    ((SyncOneshotSupplier) supplier).onAvailable(supplierCallback);
                } else {
                    assert false
                            : "Unexpected Supplier type that does not already have a value: "
                                    + supplier;
                }
            }
            // Never run callback synchronously so that behavior is consistent when suppliers are
            // ready vs not.
            if (waitingSupplierCount == 0) {
                // Hardcoded to UI thread... This should probably be "whatever thread is active",
                // but that API does not yet exist in PostTask, and ObservableSuppliers are
                // UI-thread only anyways.
                ThreadUtils.postOnUiThread(callback);
            } else {
                mWaitingCount = waitingSupplierCount;
                mCallback = callback;
            }
        }

        private void onSupplierAvailable() {
            ThreadUtils.assertOnUiThread();
            mWaitingCount--;
            assert mWaitingCount >= 0;
            if (mWaitingCount == 0) {
                assumeNonNull(mCallback);
                mCallback.run();
            }
        }
    }

    /**
     * Waits for all suppliers to have assigned values, and when that happens, notifies the
     * specified callback.
     *
     * <p>If all suppliers already have values, then the callback will be notified synchronously.
     *
     * <p>To prevent leaking objects, it is recommended to use {@link
     * org.chromium.base.CallbackController} for the {@link Runnable} callback.
     *
     * <p>May be used only on UI thread.
     *
     * @param callback The callback to be notified when all suppliers have values set.
     * @param suppliers The list of suppliers to check for values.
     */
    public static void waitForAll(Runnable callback, Supplier<?>... suppliers) {
        assert callback != null;
        new Barrier().waitForAll(callback, suppliers);
    }

    /** Casts a Supplier<@Nullable T> -> Supplier<@NonNull T> */
    public static <T> Supplier<T> asNonNull(Supplier<@Nullable T> supplier) {
        assert supplier.get() != null;
        if (BuildConfig.ENABLE_ASSERTS) {
            return () -> assertNonNull(supplier.get());
        }
        return (Supplier<T>) supplier;
    }

    @Contract("!null -> !null")
    public static <T extends @Nullable Object> @Nullable T getOrNull(@Nullable Supplier<T> sup) {
        return sup == null ? null : sup.get();
    }

    public static <T extends @Nullable Object> @NonNull T getOr(
            @Nullable Supplier<T> sup, T value) {
        T ret = sup == null ? null : sup.get();
        return ret == null ? value : ret;
    }

    public static <T extends @Nullable Boolean> boolean getOr(
            @Nullable Supplier<T> sup, boolean value) {
        Boolean ret = sup == null ? null : sup.get();
        return ret == null ? value : ret;
    }

    public static <T extends @Nullable Object> Supplier<T> of(T value) {
        return value == null ? ofNull() : () -> value;
    }

    @SuppressWarnings("unchecked")
    public static <T extends @Nullable Object> Supplier<T> ofNull() {
        return (Supplier<T>) NULL_SUPPLIER;
    }
}
