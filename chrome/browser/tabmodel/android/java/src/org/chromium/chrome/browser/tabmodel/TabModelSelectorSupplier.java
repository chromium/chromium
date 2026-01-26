// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.UnownedUserDataHost;
import org.chromium.base.UnownedUserDataKey;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.ui.base.WindowAndroid;

/** A class which manages the supplier and UnownedUserData for a {@link TabModelSelector}. */
@NullMarked
public class TabModelSelectorSupplier {
    private static final UnownedUserDataKey<MonotonicObservableSupplier<TabModelSelector>> KEY =
            new UnownedUserDataKey<>();
    private static @Nullable NonNullObservableSupplier<TabModelSelector> sInstanceForTesting;

    /** Return {@link TabModelSelector} supplier associated with the given {@link WindowAndroid}. */
    public static @Nullable MonotonicObservableSupplier<TabModelSelector> from(
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
        MonotonicObservableSupplier<TabModelSelector> supplier = from(windowAndroid);
        return supplier == null ? null : supplier.get();
    }

    /** Return the current {@link Tab} associated with {@link WindowAndroid} or null. */
    public static @Nullable Tab getCurrentTabFrom(@Nullable WindowAndroid windowAndroid) {
        TabModelSelector selector = getValueOrNullFrom(windowAndroid);
        return selector == null ? null : selector.getCurrentTab();
    }

    /**
     * Attach to the specified host.
     *
     * @param host The host to attach the supplier to.
     */
    public static void attach(
            UnownedUserDataHost host, MonotonicObservableSupplier<TabModelSelector> supplier) {
        KEY.attachToHost(host, supplier);
    }

    public static void destroy(MonotonicObservableSupplier<TabModelSelector> supplier) {
        KEY.detachFromAllHosts(supplier);
    }

    /** Sets an instance for testing. */
    public static void setInstanceForTesting(TabModelSelector tabModelSelector) {
        sInstanceForTesting = ObservableSuppliers.createNonNull(tabModelSelector);
        ResettersForTesting.register(() -> sInstanceForTesting = null);
    }

    private TabModelSelectorSupplier() {}
}
