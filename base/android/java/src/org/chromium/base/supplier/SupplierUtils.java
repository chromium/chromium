// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.supplier;

import androidx.annotation.NonNull;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;

/** Utilities for interactions with Suppliers. */
public class SupplierUtils {
    private SupplierUtils() {}

    private static class Barrier {
        private final ThreadUtils.ThreadChecker mThreadChecker = new ThreadUtils.ThreadChecker();
        private int mWaitingCount;
        private Runnable mCallback;

        void waitForAll(Runnable callback, Supplier... suppliers) {
            mThreadChecker.assertOnValidThread();
            assert mCallback == null;
            mCallback = callback;
            int waitingSupplierCount = 0;
            Callback<?> supplierCallback = (unused) -> onSupplierAvailable();
            for (Supplier<?> supplier : suppliers) {
                if (supplier.hasValue()) continue;

                waitingSupplierCount++;
                if (supplier instanceof ObservableSupplier) {
                    ObservableSupplier<?> observableSupplier = ((ObservableSupplier) supplier);
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
            mWaitingCount = waitingSupplierCount;
            notifyCallbackIfAppropriate();
        }

        private void onSupplierAvailable() {
            mThreadChecker.assertOnValidThread();
            mWaitingCount--;
            assert mWaitingCount >= 0;
            notifyCallbackIfAppropriate();
        }

        private void notifyCallbackIfAppropriate() {
            if (mWaitingCount != 0) return;
            if (mCallback == null) return;
            mCallback.run();
            mCallback = null;
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
     * <p>Not thread safe. All passed in suppliers must be notified on the same thread this method
     * is called.
     *
     * @param callback The callback to be notified when all suppliers have values set.
     * @param suppliers The list of suppliers to check for values.
     */
    public static void waitForAll(@NonNull Runnable callback, Supplier... suppliers) {
        assert callback != null;
        new Barrier().waitForAll(callback, suppliers);
    }
}
