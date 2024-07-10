// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import androidx.annotation.Nullable;

import org.chromium.base.UnownedUserDataKey;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.UnownedUserDataSupplier;
import org.chromium.ui.base.WindowAndroid;

/**
 * A {@link UnownedUserDataSupplier} which manages the supplier and UnownedUserData for a {@link
 * ContextualSearchManager}.
 */
public class ContextualSearchManagerSupplier
        extends UnownedUserDataSupplier<ContextualSearchManager> {
    private static final UnownedUserDataKey<ContextualSearchManagerSupplier> KEY =
            new UnownedUserDataKey<>(ContextualSearchManagerSupplier.class);

    /**
     * Return {@link ContextualSearchManager} supplier associated with the given {@link
     * WindowAndroid}.
     */
    @Nullable
    public static ObservableSupplier<ContextualSearchManager> from(WindowAndroid windowAndroid) {
        return KEY.retrieveDataFromHost(windowAndroid.getUnownedUserDataHost());
    }

    /** Constructs a ContextualSearchManagerSupplier and attaches it to the {@link WindowAndroid} */
    public ContextualSearchManagerSupplier() {
        super(KEY);
    }
}
