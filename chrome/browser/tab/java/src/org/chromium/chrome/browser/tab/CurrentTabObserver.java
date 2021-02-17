// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.CallbackController;
import org.chromium.base.supplier.ObservableSupplier;

/**
 * TabObserver that keeps switching to always observe the current tab. A stop-gap solution
 * to ActivityTabTabObserver for classes that cannot reference it directly.
 * TODO(crbug.com/1146871): Just switch to ActivityTabTabObserver once ActivityTabProvider
 *                  gets modularized.
 */
public class CurrentTabObserver {
    private final ObservableSupplier<Tab> mTabSupplier;
    private final TabObserver mTabObserver;
    private final Callback<Tab> mTabSupplierCallback;
    private CallbackController mCallbackController;
    private Tab mTab;

    /**
     * @param tabSupplier An observable supplier of the current {@link Tab}. NOT to be owned
     *        by this class, and should be destroyed by callsite later.
     * @param tabObserver {@link TabObserver} that we want to observe the current tab with.
     *        Owned by this class.
     * @param swapCallback Callback to invoke when the current tab is swapped.
     */
    public CurrentTabObserver(@NonNull ObservableSupplier<Tab> tabSupplier,
            @NonNull TabObserver tabObserver, @Nullable Callback<Tab> swapCallback) {
        mTabSupplier = tabSupplier;
        mTabObserver = tabObserver;
        mCallbackController = new CallbackController();
        mTabSupplierCallback = mCallbackController.makeCancelable((tab) -> {
            if (mTab == tab) return;
            if (mTab != null) mTab.removeObserver(mTabObserver);
            mTab = tab;
            if (mTab != null) mTab.addObserver(mTabObserver);
            if (swapCallback != null) swapCallback.onResult(tab);
        });
        mTabSupplier.addObserver(mTabSupplierCallback);
    }

    /** Trigger the event callback for this observer with the current tab. */
    public void triggerWithCurrentTab() {
        mTabSupplierCallback.onResult(mTabSupplier.get());
    }

    /** Destroy the current tab observer. This should be called after use. */
    public void destroy() {
        if (mTab != null) mTab.removeObserver(mTabObserver);
        mTabSupplier.removeObserver(mTabSupplierCallback);
        mCallbackController.destroy();
        mCallbackController = null;
    }
}
