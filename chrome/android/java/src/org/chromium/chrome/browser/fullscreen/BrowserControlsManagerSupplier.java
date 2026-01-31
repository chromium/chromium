// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.fullscreen;

import org.chromium.base.UnownedUserDataHost;
import org.chromium.base.UnownedUserDataKey;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.base.WindowAndroid;

/** A class which manages the supplier and UnownedUserData for a {@link BrowserControlsManager}. */
@NullMarked
public class BrowserControlsManagerSupplier {
    private static final UnownedUserDataKey<MonotonicObservableSupplier<BrowserControlsManager>>
            KEY = new UnownedUserDataKey<>();

    /** Return {@link TabModelSelector} supplier associated with the given {@link WindowAndroid}. */
    public static @Nullable MonotonicObservableSupplier<BrowserControlsManager> from(
            @Nullable WindowAndroid windowAndroid) {
        if (windowAndroid == null) return null;
        return KEY.retrieveDataFromHost(windowAndroid.getUnownedUserDataHost());
    }

    /**
     * Retrieves an {@link MonotonicObservableSupplier} from the given host. Real implementations should
     * use {@link WindowAndroid}.
     */
    public static @Nullable BrowserControlsManager getValueOrNullFrom(
            @Nullable WindowAndroid windowAndroid) {
        MonotonicObservableSupplier<BrowserControlsManager> supplier = from(windowAndroid);
        return supplier == null ? null : supplier.get();
    }

    /**
     * Attach to the specified host.
     *
     * @param host The host to attach the supplier to.
     */
    public static void attach(
            UnownedUserDataHost host,
            MonotonicObservableSupplier<BrowserControlsManager> supplier) {
        KEY.attachToHost(host, supplier);
    }

    public static void destroy(MonotonicObservableSupplier<BrowserControlsManager> supplier) {
        KEY.detachFromAllHosts(supplier);
    }

    private BrowserControlsManagerSupplier() {}
}
