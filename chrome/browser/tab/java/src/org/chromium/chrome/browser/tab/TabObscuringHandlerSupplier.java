// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import androidx.annotation.Nullable;

import org.chromium.base.UnownedUserDataKey;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.UnownedUserDataSupplier;
import org.chromium.ui.base.WindowAndroid;

/**
 * A {@link UnownedUserDataSupplier} which manages the supplier and UnownedUserData for a
 * {@link TabObscuringHandler}.
 */
public class TabObscuringHandlerSupplier extends UnownedUserDataSupplier<TabObscuringHandler> {
    private static final UnownedUserDataKey<TabObscuringHandlerSupplier> KEY =
            new UnownedUserDataKey<TabObscuringHandlerSupplier>(TabObscuringHandlerSupplier.class);

    /**
     * Retrieves an {@link ObservableSupplier} from the given host. Real implementations should
     * use {@link WindowAndroid}.
     */
    public static @Nullable TabObscuringHandler getValueOrNullFrom(
            @Nullable WindowAndroid windowAndroid) {
        if (windowAndroid == null) return null;
        TabObscuringHandlerSupplier supplier =
                KEY.retrieveDataFromHost(windowAndroid.getUnownedUserDataHost());
        return supplier == null ? null : supplier.get();
    }

    /** Constructs a TabObscuringHandlerSupplier and attaches it to the {@link WindowAndroid} */
    public TabObscuringHandlerSupplier() {
        super(KEY);
    }
}
