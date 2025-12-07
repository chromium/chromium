// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import org.chromium.base.Callback;
import org.chromium.base.CallbackController;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * TabObserver that keeps switching to always observe the "current" tab. The current tab is decided
 * by the provided ObservableSupplier<Tab>.
 */
@NullMarked
public class CurrentTabObserver {
    private final ObservableSupplier<@Nullable Tab> mTabSupplier;
    private final TabObserver mTabObserver;
    private final Callback<@Nullable Tab> mTabSupplierCallback;
    private CallbackController mCallbackController;
    private @Nullable Tab mTab;

    /**
     * @see #CurrentTabObserver(ObservableSupplier, TabObserver, Callback)
     */
    public CurrentTabObserver(
            ObservableSupplier<@Nullable Tab> tabSupplier, TabObserver tabObserver) {
        this(tabSupplier, tabObserver, null);
    }

    /**
     * @param tabSupplier An observable supplier of the current {@link Tab}. NOT to be owned by this
     *     class, and should be destroyed by callsite later.
     * @param tabObserver {@link TabObserver} that we want to observe the current tab with. Owned by
     *     this class.
     * @param swapCallback Callback to invoke when the current tab is swapped.
     */
    public CurrentTabObserver(
            ObservableSupplier<@Nullable Tab> tabSupplier,
            TabObserver tabObserver,
            @Nullable Callback<@Nullable Tab> swapCallback) {
        mTabSupplier = tabSupplier;
        mTabObserver = tabObserver;
        mCallbackController = new CallbackController();
        Callback<@Nullable Tab> supplierCallback =
                tab -> {
                    if (mTab == tab) return;
                    if (mTab != null) mTab.removeObserver(mTabObserver);
                    mTab = tab;
                    if (mTab != null) mTab.addObserver(mTabObserver);
                    if (swapCallback != null) swapCallback.onResult(tab);
                };
        mTabSupplierCallback = mCallbackController.makeCancelable(supplierCallback);
        mTabSupplier.addObserver(mTabSupplierCallback);
    }

    /** Trigger the event callback for this observer with the current tab. */
    public void triggerWithCurrentTab() {
        mTabSupplierCallback.onResult(mTabSupplier.get());
    }

    /** Destroy the current tab observer. This should be called after use. */
    @SuppressWarnings("NullAway")
    public void destroy() {
        if (mTab != null) mTab.removeObserver(mTabObserver);
        mTabSupplier.removeObserver(mTabSupplierCallback);
        mCallbackController.destroy();
        mCallbackController = null;
    }
}
