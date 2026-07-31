// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.UnownedUserDataHost;
import org.chromium.base.UnownedUserDataKey;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.base.WindowAndroid;

/** A class which manages the supplier and UnownedUserData for a {@link CompositorViewHolder}. */
@NullMarked
public class CompositorViewHolderSupplier {
    private static final UnownedUserDataKey<MonotonicObservableSupplier<CompositorViewHolder>> KEY =
            new UnownedUserDataKey<>();
    private static @Nullable NonNullObservableSupplier<CompositorViewHolder> sInstanceForTesting;

    /**
     * Return {@link CompositorViewHolder} supplier associated with the given {@link WindowAndroid}.
     */
    public static @Nullable MonotonicObservableSupplier<CompositorViewHolder> from(
            @Nullable WindowAndroid windowAndroid) {
        if (sInstanceForTesting != null) return sInstanceForTesting;
        if (windowAndroid == null) return null;
        return KEY.retrieveDataFromHost(windowAndroid.getUnownedUserDataHost());
    }

    /**
     * Return {@link CompositorViewHolder} associated with the given {@link WindowAndroid} or null
     * if none exists.
     */
    public static @Nullable CompositorViewHolder getValueOrNullFrom(
            @Nullable WindowAndroid windowAndroid) {
        MonotonicObservableSupplier<CompositorViewHolder> supplier = from(windowAndroid);
        return supplier == null ? null : supplier.get();
    }

    /**
     * Attach to the specified host.
     *
     * @param host The host to attach the supplier to.
     * @param supplier The supplier to attach.
     */
    public static void attach(
            UnownedUserDataHost host, MonotonicObservableSupplier<CompositorViewHolder> supplier) {
        KEY.attachToHost(host, supplier);
    }

    /**
     * Detach from all hosts.
     *
     * @param supplier The supplier to destroy.
     */
    public static void destroy(MonotonicObservableSupplier<CompositorViewHolder> supplier) {
        KEY.detachFromAllHosts(supplier);
    }

    /** Sets an instance for testing. */
    public static void setInstanceForTesting(CompositorViewHolder compositorViewHolder) {
        sInstanceForTesting = ObservableSuppliers.createNonNull(compositorViewHolder);
        ResettersForTesting.register(() -> sInstanceForTesting = null);
    }

    private CompositorViewHolderSupplier() {}
}
