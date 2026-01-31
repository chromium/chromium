// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.UnownedUserDataHost;
import org.chromium.base.UnownedUserDataKey;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.base.WindowAndroid;

/** A class which manages the supplier and UnownedUserData for a {@link ShareDelegate}. */
@NullMarked
public class ShareDelegateSupplier {
    private static final UnownedUserDataKey<MonotonicObservableSupplier<ShareDelegate>> KEY =
            new UnownedUserDataKey<>();

    private static @Nullable MonotonicObservableSupplier<ShareDelegate> sInstanceForTesting;

    /** Return {@link ShareDelegate} supplier associated with the given {@link WindowAndroid}. */
    public static @Nullable MonotonicObservableSupplier<ShareDelegate> from(
            WindowAndroid windowAndroid) {
        if (sInstanceForTesting != null) return sInstanceForTesting;
        return KEY.retrieveDataFromHost(windowAndroid.getUnownedUserDataHost());
    }

    /**
     * Attach to the specified host.
     *
     * @param host The host to attach the supplier to.
     */
    public static void attach(
            UnownedUserDataHost host, MonotonicObservableSupplier<ShareDelegate> supplier) {
        KEY.attachToHost(host, supplier);
    }

    public static void destroy(MonotonicObservableSupplier<ShareDelegate> supplier) {
        KEY.detachFromAllHosts(supplier);
    }

    public static void setInstanceForTesting(
            MonotonicObservableSupplier<ShareDelegate> instanceForTesting) {
        sInstanceForTesting = instanceForTesting;
        ResettersForTesting.register(() -> sInstanceForTesting = null);
    }

    private ShareDelegateSupplier() {}
}
