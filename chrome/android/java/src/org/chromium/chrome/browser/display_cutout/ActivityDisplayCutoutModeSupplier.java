// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.display_cutout;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.UnownedUserDataKey;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.UnownedUserDataSupplier;
import org.chromium.ui.base.WindowAndroid;

/**
 * Provides activity-wide display cutout mode override.
 *
 * If the activity uses a custom display cutout mode, ActivityDisplayCutoutModeSupplier should be
 * attached to WindowAndroid prior to the first tab getting attached to WindowAndroid.
 */
public class ActivityDisplayCutoutModeSupplier extends UnownedUserDataSupplier<Integer> {
    /** The key for accessing this object on an {@link org.chromium.base.UnownedUserDataHost}. */
    private static final UnownedUserDataKey<ActivityDisplayCutoutModeSupplier> KEY =
            new UnownedUserDataKey<>(ActivityDisplayCutoutModeSupplier.class);

    private static ObservableSupplierImpl<Integer> sInstanceForTesting;

    public static @Nullable ObservableSupplier<Integer> from(@NonNull WindowAndroid window) {
        if (sInstanceForTesting != null) return sInstanceForTesting;
        return KEY.retrieveDataFromHost(window.getUnownedUserDataHost());
    }

    public ActivityDisplayCutoutModeSupplier() {
        super(KEY);
    }

    /** Sets an instance for testing. */
    public static void setInstanceForTesting(Integer mode) {
        if (sInstanceForTesting == null) {
            sInstanceForTesting = new ObservableSupplierImpl<>();
        }
        sInstanceForTesting.set(mode);
    }
}
