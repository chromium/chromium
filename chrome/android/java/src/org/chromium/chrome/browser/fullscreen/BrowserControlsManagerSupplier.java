// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.fullscreen;

import androidx.annotation.Nullable;

import org.chromium.base.UnownedUserDataKey;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.UnownedUserDataSupplier;
import org.chromium.ui.base.WindowAndroid;

/**
 * A {@link UnownedUserDataSupplier} which manages the supplier and UnownedUserData for a
 * {@link BrowserControlsManager}.
 */
public class BrowserControlsManagerSupplier
        extends UnownedUserDataSupplier<BrowserControlsManager> {
    private static final UnownedUserDataKey<BrowserControlsManagerSupplier> KEY =
            new UnownedUserDataKey<BrowserControlsManagerSupplier>(
                    BrowserControlsManagerSupplier.class);

    /** Return {@link TabModelSelector} supplier associated with the given {@link WindowAndroid}. */
    public static ObservableSupplier<BrowserControlsManager> from(
            @Nullable WindowAndroid windowAndroid) {
        if (windowAndroid == null) return null;
        return KEY.retrieveDataFromHost(windowAndroid.getUnownedUserDataHost());
    }

    /**
     * Retrieves an {@link ObservableSupplier} from the given host. Real implementations should
     * use {@link WindowAndroid}.
     */
    public static @Nullable BrowserControlsManager getValueOrNullFrom(
            @Nullable WindowAndroid windowAndroid) {
        ObservableSupplier<BrowserControlsManager> supplier = from(windowAndroid);
        return supplier == null ? null : supplier.get();
    }

    /** Constructs a BrowserControlsManagerSupplier and attaches it to the {@link WindowAndroid} */
    public BrowserControlsManagerSupplier() {
        super(KEY);
    }
}
