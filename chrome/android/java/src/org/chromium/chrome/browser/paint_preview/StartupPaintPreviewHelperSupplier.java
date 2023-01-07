// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.paint_preview;

import org.chromium.base.UnownedUserDataKey;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.UnownedUserDataSupplier;
import org.chromium.ui.base.WindowAndroid;

/**
 * A {@link UnownedUserDataSupplier} which manages the supplier and UnownedUserData for a
 * {@link StartupPaintPreviewHelper}.
 */
public class StartupPaintPreviewHelperSupplier
        extends UnownedUserDataSupplier<StartupPaintPreviewHelper> {
    private static final UnownedUserDataKey<StartupPaintPreviewHelperSupplier> KEY =
            new UnownedUserDataKey<>(StartupPaintPreviewHelperSupplier.class);

    /**
     * Return {@link StartupPaintPreviewHelper} supplier associated with the given {@link
     * WindowAndroid}.
     */
    public static ObservableSupplier<StartupPaintPreviewHelper> from(WindowAndroid windowAndroid) {
        return KEY.retrieveDataFromHost(windowAndroid.getUnownedUserDataHost());
    }

    /**
     * Constructs a StartupPaintPreviewHelperSupplier and attaches it to the {@link
     * WindowAndroid}
     */
    public StartupPaintPreviewHelperSupplier() {
        super(KEY);
    }
}
