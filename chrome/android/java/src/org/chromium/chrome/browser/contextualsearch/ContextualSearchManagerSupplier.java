// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import org.chromium.base.UnownedUserDataHost;
import org.chromium.base.UnownedUserDataKey;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.base.WindowAndroid;

/**
 * A {@link MonotonicObservableSupplier} which manages the supplier and UnownedUserData for a {@link
 * ContextualSearchManager}.
 */
@NullMarked
public class ContextualSearchManagerSupplier {
    private static final UnownedUserDataKey<MonotonicObservableSupplier<ContextualSearchManager>>
            KEY = new UnownedUserDataKey<>();

    /**
     * Return {@link ContextualSearchManager} supplier associated with the given {@link
     * WindowAndroid}.
     */
    public static @Nullable MonotonicObservableSupplier<ContextualSearchManager> from(
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
            MonotonicObservableSupplier<ContextualSearchManager> supplier) {
        KEY.attachToHost(host, supplier);
    }

    public static void destroy(MonotonicObservableSupplier<ContextualSearchManager> supplier) {
        KEY.detachFromAllHosts(supplier);
    }

    private ContextualSearchManagerSupplier() {}
}
