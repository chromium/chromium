// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.paint_preview;

import org.chromium.base.UnownedUserDataKey;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.UnownedUserDataSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.base.WindowAndroid;

/**
 * A {@link UnownedUserDataSupplier} which manages the supplier and UnownedUserData for a {@link
 * StartupPaintPreviewHelper}.
 */
@NullMarked
public class StartupPaintPreviewHelperSupplier
        extends UnownedUserDataSupplier<StartupPaintPreviewHelper> {
    private static final UnownedUserDataKey<StartupPaintPreviewHelperSupplier> KEY =
            new UnownedUserDataKey<>();

    /**
     * Return {@link StartupPaintPreviewHelper} supplier associated with the given {@link
     * WindowAndroid} or null if not yet initialized.
     */
    public static @Nullable ObservableSupplier<StartupPaintPreviewHelper> from(
            WindowAndroid windowAndroid) {
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
