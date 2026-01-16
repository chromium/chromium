// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import org.chromium.base.UnownedUserDataHost;
import org.chromium.base.UnownedUserDataKey;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.base.WindowAndroid;

/** A class which manages the supplier and UnownedUserData for a {@link TabObscuringHandler}. */
@NullMarked
public class TabObscuringHandlerSupplier {
    private static final UnownedUserDataKey<MonotonicObservableSupplier<TabObscuringHandler>> KEY =
            new UnownedUserDataKey<>();

    /**
     * Retrieves an {@link MonotonicObservableSupplier} from the given host. Real implementations should use
     * {@link WindowAndroid}.
     */
    public static @Nullable TabObscuringHandler getValueOrNullFrom(
            @Nullable WindowAndroid windowAndroid) {
        if (windowAndroid == null) return null;
        MonotonicObservableSupplier<TabObscuringHandler> supplier =
                KEY.retrieveDataFromHost(windowAndroid.getUnownedUserDataHost());
        return supplier == null ? null : supplier.get();
    }

    /**
     * Attach to the specified host.
     *
     * @param host The host to attach the supplier to.
     */
    public static void attach(
            UnownedUserDataHost host, MonotonicObservableSupplier<TabObscuringHandler> supplier) {
        KEY.attachToHost(host, supplier);
    }

    public static void destroy(MonotonicObservableSupplier<TabObscuringHandler> supplier) {
        KEY.detachFromAllHosts(supplier);
    }

    private TabObscuringHandlerSupplier() {}
}
