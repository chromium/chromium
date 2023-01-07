// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import org.chromium.base.UnownedUserDataKey;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.UnownedUserDataSupplier;
import org.chromium.ui.base.WindowAndroid;

/**
 * An {@link UnownedUserDataSupplier} which manages the supplier and UnownedUserData for a
 * {@link TabCreatorManager}.
 */
public class TabCreatorManagerSupplier extends UnownedUserDataSupplier<TabCreatorManager> {
    private static final UnownedUserDataKey<TabCreatorManagerSupplier> KEY =
            new UnownedUserDataKey<>(TabCreatorManagerSupplier.class);

    /**
     * Constructs an {@link TabCreatorManagerSupplier} and attaches it to the
     * {@link WindowAndroid}.
     */
    public TabCreatorManagerSupplier() {
        super(KEY);
    }

    /**
     * @return The {@link TabCreatorManager} supplier associated with the given
     * {@link WindowAndroid}.
     */
    public static ObservableSupplier<TabCreatorManager> from(WindowAndroid windowAndroid) {
        return KEY.retrieveDataFromHost(windowAndroid.getUnownedUserDataHost());
    }
}
