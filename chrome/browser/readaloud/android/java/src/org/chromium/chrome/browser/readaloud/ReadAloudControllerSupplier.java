// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud;

import org.chromium.base.UnownedUserDataHost;
import org.chromium.base.UnownedUserDataKey;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.base.WindowAndroid;

/** A class which manages the supplier and UnownedUserData for a {@link ReadAloudController}. */
@NullMarked
public class ReadAloudControllerSupplier {
    private static final UnownedUserDataKey<MonotonicObservableSupplier<ReadAloudController>> KEY =
            new UnownedUserDataKey<>();

    /**
     * Return {@link ReadAloudController} supplier associated with the given {@link WindowAndroid}.
     */
    public static @Nullable MonotonicObservableSupplier<ReadAloudController> from(
            WindowAndroid windowAndroid) {
        return KEY.retrieveDataFromHost(windowAndroid.getUnownedUserDataHost());
    }

    /**
     * Attach to the specified host.
     *
     * @param host The host to attach the supplier to.
     */
    public static void attach(
            UnownedUserDataHost host, MonotonicObservableSupplier<ReadAloudController> supplier) {
        KEY.attachToHost(host, supplier);
    }

    public static void destroy(MonotonicObservableSupplier<ReadAloudController> supplier) {
        KEY.detachFromAllHosts(supplier);
    }

    private ReadAloudControllerSupplier() {}
}
