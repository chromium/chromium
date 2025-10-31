// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.UnownedUserDataKey;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.UnownedUserDataSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.ui.base.WindowAndroid;

/**
 * A {@link UnownedUserDataSupplier} which manages the supplier and UnownedUserData for a {@link
 * TabModelSelector}.
 */
@NullMarked
public class TabModelSelectorSupplier extends UnownedUserDataSupplier<TabModelSelector> {
    private static final UnownedUserDataKey<TabModelSelectorSupplier> KEY =
            new UnownedUserDataKey<>();
    private static @Nullable ObservableSupplierImpl<TabModelSelector> sInstanceForTesting;

    /** Return {@link TabModelSelector} supplier associated with the given {@link WindowAndroid}. */
    public static @Nullable ObservableSupplier<TabModelSelector> from(
            @Nullable WindowAndroid windowAndroid) {
        if (sInstanceForTesting != null) return sInstanceForTesting;
        if (windowAndroid == null) return null;
        return KEY.retrieveDataFromHost(windowAndroid.getUnownedUserDataHost());
    }

    /**
     * Return {@link TabModelSelector} associated with the given {@link WindowAndroid} or null if
     * none exists.
     */
    public static @Nullable TabModelSelector getValueOrNullFrom(
            @Nullable WindowAndroid windowAndroid) {
        ObservableSupplier<TabModelSelector> supplier = from(windowAndroid);
        return supplier == null ? null : supplier.get();
    }

    /** Return the current {@link Tab} associated with {@link WindowAndroid} or null. */
    public static @Nullable Tab getCurrentTabFrom(@Nullable WindowAndroid windowAndroid) {
        TabModelSelector selector = getValueOrNullFrom(windowAndroid);
        return selector == null ? null : selector.getCurrentTab();
    }

    /** Constructs a TabModelSelectorSupplier and attaches it to the {@link WindowAndroid} */
    public TabModelSelectorSupplier() {
        super(KEY);
    }

    /** Sets an instance for testing. */
    public static void setInstanceForTesting(TabModelSelector tabModelSelector) {
        sInstanceForTesting = new ObservableSupplierImpl<>(tabModelSelector);
        ResettersForTesting.register(() -> sInstanceForTesting = null);
    }
}
