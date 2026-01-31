// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.paint_preview;

import org.chromium.base.UnownedUserDataHost;
import org.chromium.base.UnownedUserDataKey;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.base.WindowAndroid;

/**
 * A class which manages the supplier and UnownedUserData for a {@link StartupPaintPreviewHelper}.
 */
@NullMarked
public class StartupPaintPreviewHelperSupplier {
    private static final UnownedUserDataKey<MonotonicObservableSupplier<StartupPaintPreviewHelper>>
            KEY = new UnownedUserDataKey<>();

    /**
     * Return {@link StartupPaintPreviewHelper} supplier associated with the given {@link
     * WindowAndroid} or null if not yet initialized.
     */
    public static @Nullable MonotonicObservableSupplier<StartupPaintPreviewHelper> from(
            WindowAndroid windowAndroid) {
        return KEY.retrieveDataFromHost(windowAndroid.getUnownedUserDataHost());
    }

    /**
     * Attach to the specified host.
     *
     * @param host The host to attach the supplier to.
     */
    public static void attach(
            UnownedUserDataHost host,
            MonotonicObservableSupplier<StartupPaintPreviewHelper> supplier) {
        KEY.attachToHost(host, supplier);
    }

    public static void destroy(MonotonicObservableSupplier<StartupPaintPreviewHelper> supplier) {
        KEY.detachFromAllHosts(supplier);
    }

    private StartupPaintPreviewHelperSupplier() {}
}
