// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.UnownedUserDataKey;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.UnownedUserDataSupplier;
import org.chromium.ui.base.WindowAndroid;

/**
 * A {@link UnownedUserDataSupplier} which manages the supplier and UnownedUserData for a
 * {@link ShareDelegate}.
 */
public class ShareDelegateSupplier extends UnownedUserDataSupplier<ShareDelegate> {
    private static final UnownedUserDataKey<ShareDelegateSupplier> KEY =
            new UnownedUserDataKey<ShareDelegateSupplier>(ShareDelegateSupplier.class);

    private static ShareDelegateSupplier sInstanceForTesting;

    /** Return {@link ShareDelegate} supplier associated with the given {@link WindowAndroid}. */
    public static ObservableSupplier<ShareDelegate> from(WindowAndroid windowAndroid) {
        if (sInstanceForTesting != null) return sInstanceForTesting;
        return KEY.retrieveDataFromHost(windowAndroid.getUnownedUserDataHost());
    }

    /** Constructs a ShareDelegateSupplier and attaches it to the {@link WindowAndroid} */
    public ShareDelegateSupplier() {
        super(KEY);
    }

    public static void setInstanceForTesting(ShareDelegateSupplier instanceForTesting) {
        sInstanceForTesting = instanceForTesting;
        ResettersForTesting.register(() -> sInstanceForTesting = null);
    }
}
