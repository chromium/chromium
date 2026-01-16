// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.display_cutout;

import org.chromium.base.UnownedUserDataHost;
import org.chromium.base.UnownedUserDataKey;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.base.WindowAndroid;

/**
 * Provides activity-wide display cutout mode override.
 *
 * <p>If the activity uses a custom display cutout mode, ActivityDisplayCutoutModeSupplier should be
 * attached to WindowAndroid prior to the first tab getting attached to WindowAndroid.
 */
@NullMarked
public class ActivityDisplayCutoutModeSupplier {
    /** The key for accessing this object on an {@link org.chromium.base.UnownedUserDataHost}. */
    private static final UnownedUserDataKey<MonotonicObservableSupplier<Integer>> KEY =
            new UnownedUserDataKey<>();

    private static @Nullable ObservableSupplierImpl<Integer> sInstanceForTesting;

    public static @Nullable MonotonicObservableSupplier<Integer> from(WindowAndroid window) {
        if (sInstanceForTesting != null) return sInstanceForTesting;
        return KEY.retrieveDataFromHost(window.getUnownedUserDataHost());
    }

    /**
     * Attach to the specified host.
     *
     * @param host The host to attach the supplier to.
     */
    public static void attach(
            UnownedUserDataHost host, MonotonicObservableSupplier<Integer> supplier) {
        KEY.attachToHost(host, supplier);
    }

    public static void destroy(MonotonicObservableSupplier<Integer> supplier) {
        KEY.detachFromAllHosts(supplier);
    }

    /** Sets an instance for testing. */
    public static void setInstanceForTesting(Integer mode) {
        if (sInstanceForTesting == null) {
            sInstanceForTesting = new ObservableSupplierImpl<>();
        }
        sInstanceForTesting.set(mode);
    }

    private ActivityDisplayCutoutModeSupplier() {}
}
