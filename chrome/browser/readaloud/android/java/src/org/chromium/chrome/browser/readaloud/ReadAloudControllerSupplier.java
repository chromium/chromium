// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud;

import androidx.annotation.Nullable;

import org.chromium.base.UnownedUserDataKey;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.UnownedUserDataSupplier;
import org.chromium.ui.base.WindowAndroid;

/**
 * A {@link UnownedUserDataSupplier} which manages the supplier and UnownedUserData for a {@link
 * ReadAloudController}.
 */
public class ReadAloudControllerSupplier extends UnownedUserDataSupplier<ReadAloudController> {
    private static final UnownedUserDataKey<ReadAloudControllerSupplier> KEY =
            new UnownedUserDataKey<>(ReadAloudControllerSupplier.class);

    /**
     * Return {@link ReadAloudController} supplier associated with the given {@link WindowAndroid}.
     */
    @Nullable
    public static ObservableSupplier<ReadAloudController> from(WindowAndroid windowAndroid) {
        return KEY.retrieveDataFromHost(windowAndroid.getUnownedUserDataHost());
    }

    /** Constructs a ReadAloudControllerSupplier and attaches it to the {@link WindowAndroid} */
    public ReadAloudControllerSupplier() {
        super(KEY);
    }
}
