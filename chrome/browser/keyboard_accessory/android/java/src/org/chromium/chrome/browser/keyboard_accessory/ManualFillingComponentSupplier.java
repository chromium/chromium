// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory;

import org.chromium.base.UnownedUserDataHost;
import org.chromium.base.UnownedUserDataKey;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.base.WindowAndroid;

/** A class which manages the supplier and UnownedUserData for a {@link ManualFillingComponent}. */
@NullMarked
public class ManualFillingComponentSupplier {
    private static final UnownedUserDataKey<ObservableSupplier<ManualFillingComponent>> KEY =
            new UnownedUserDataKey<>();

    /**
     * Return {@link ManualFillingComponent} supplier associated with the given {@link
     * WindowAndroid}.
     */
    public static @Nullable ObservableSupplier<ManualFillingComponent> from(
            WindowAndroid windowAndroid) {
        return KEY.retrieveDataFromHost(windowAndroid.getUnownedUserDataHost());
    }

    /**
     * Attach to the specified host.
     *
     * @param host The host to attach the supplier to.
     */
    public static void attach(
            UnownedUserDataHost host, ObservableSupplier<ManualFillingComponent> supplier) {
        KEY.attachToHost(host, supplier);
    }

    public static void destroy(ObservableSupplier<ManualFillingComponent> supplier) {
        KEY.detachFromAllHosts(supplier);
    }

    private ManualFillingComponentSupplier() {}
}
