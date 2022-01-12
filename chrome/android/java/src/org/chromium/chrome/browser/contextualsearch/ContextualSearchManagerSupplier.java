// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import androidx.annotation.Nullable;

import org.chromium.base.UnownedUserDataKey;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.UnownedUserDataSupplier;
import org.chromium.ui.base.WindowAndroid;

/**
 * A {@link UnownedUserDataSupplier} which manages the supplier and UnownedUserData for a
 * {@link ContextualSearchManager}.
 */
public class ContextualSearchManagerSupplier
        extends UnownedUserDataSupplier<ContextualSearchManager> {
    private static final UnownedUserDataKey<ContextualSearchManagerSupplier> KEY =
            new UnownedUserDataKey<ContextualSearchManagerSupplier>(
                    ContextualSearchManagerSupplier.class);

    /**
     * Returns {@link ContextualSearchManager} supplier associated with the given {@link
     * WindowAndroid} or {@code null}.
     */
    public static @Nullable ObservableSupplier<ContextualSearchManager> from(
            @Nullable WindowAndroid windowAndroid) {
        if (windowAndroid == null) return null;
        return KEY.retrieveDataFromHost(windowAndroid.getUnownedUserDataHost());
    }

    /** Retrieves a {@link ContextualSearchManager} from {@link WindowAndroid}.  */
    public static @Nullable ContextualSearchManager getValueOrNullFrom(
            @Nullable WindowAndroid windowAndroid) {
        ObservableSupplier<ContextualSearchManager> supplier = from(windowAndroid);
        return supplier == null ? null : supplier.get();
    }

    public ContextualSearchManagerSupplier() {
        super(KEY);
    }
}
