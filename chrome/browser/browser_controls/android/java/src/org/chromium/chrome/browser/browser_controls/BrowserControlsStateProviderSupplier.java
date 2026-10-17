// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browser_controls;

import org.chromium.base.UnownedUserDataHost;
import org.chromium.base.UnownedUserDataKey;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.base.WindowAndroid;

/**
 * Makes the activity's {@link BrowserControlsStateProvider} available through its {@link
 * WindowAndroid}, so that code outside of chrome_java can read browser controls state (e.g. the
 * controls position) without depending on the activity.
 */
@NullMarked
public final class BrowserControlsStateProviderSupplier {
    private static final UnownedUserDataKey<
                    MonotonicObservableSupplier<? extends BrowserControlsStateProvider>>
            KEY = new UnownedUserDataKey<>();

    /**
     * Returns the {@link BrowserControlsStateProvider} supplier associated with the given window,
     * or null if the window is null or nothing is attached. Use this to observe when the provider
     * becomes available.
     */
    public static @Nullable MonotonicObservableSupplier<? extends BrowserControlsStateProvider>
            from(@Nullable WindowAndroid windowAndroid) {
        if (windowAndroid == null) return null;
        return KEY.retrieveDataFromHost(windowAndroid.getUnownedUserDataHost());
    }

    /**
     * Returns the {@link BrowserControlsStateProvider} associated with the given window, or null if
     * the window is null, nothing is attached, or the provider has not been created yet.
     */
    public static @Nullable BrowserControlsStateProvider getValueOrNullFrom(
            @Nullable WindowAndroid windowAndroid) {
        MonotonicObservableSupplier<? extends BrowserControlsStateProvider> supplier =
                from(windowAndroid);
        return supplier == null ? null : supplier.get();
    }

    /**
     * Attaches a supplier to the given host.
     *
     * @param host The host to attach to.
     * @param supplier The supplier to attach. The same instance must be passed to {@link #destroy}.
     */
    public static void attach(
            UnownedUserDataHost host,
            MonotonicObservableSupplier<? extends BrowserControlsStateProvider> supplier) {
        KEY.attachToHost(host, supplier);
    }

    /**
     * Detaches the given supplier from any host it is attached to.
     *
     * @param supplier The supplier previously passed to {@link #attach}.
     */
    public static void destroy(
            MonotonicObservableSupplier<? extends BrowserControlsStateProvider> supplier) {
        KEY.detachFromAllHosts(supplier);
    }

    private BrowserControlsStateProviderSupplier() {}
}
