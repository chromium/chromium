// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import androidx.annotation.CallSuper;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.ObservableSupplier;

/**
 * A utility class for observing a {@link Tab} changing via {@link TabObserver}.<br>
 * When the supplier's tab changes, the observer is switched to that Tab and
 * {@link #onObservingDifferentTab} is called to notify the instance of the change.<br>
 * This can be used with an {@code ActivityTabProvider} to track which Tab is the current Tab for an
 * Activity.
 */
public class TabSupplierObserver extends EmptyTabObserver implements Destroyable {
    /** A handle to the tab supplier. */
    private final ObservableSupplier<Tab> mTabSupplier;

    /** An observer to watch for a changing tab and move this tab observer. */
    private final Callback<Tab> mTabObserver;

    /** The current tab. */
    private Tab mTab;

    public TabSupplierObserver(ObservableSupplier<Tab> tabSupplier) {
        this(tabSupplier, false);
    }

    /**
     * Create a new {@link TabObserver} that only observes the tab from the given supplier.
     * It doesn't trigger for the initial tab being attached to after creation.
     * @param tabSupplier An {@link ObservableSupplier} to get the current tab.
     * @param shouldTrigger Whether the observer should be triggered for the initial tab after
     * creation.
     */
    public TabSupplierObserver(ObservableSupplier<Tab> tabSupplier, boolean shouldTrigger) {
        mTabSupplier = tabSupplier;
        mTabObserver =
                (tab) -> {
                    updateObservedTab(tab);
                    onObservingDifferentTab(tab);
                };

        addObserverToTabSupplier();
        if (shouldTrigger) onObservingDifferentTab(tabSupplier.get());

        updateObservedTabToCurrent();
    }

    /**
     * Update the tab being observed.
     * @param newTab The new tab to observe.
     */
    private void updateObservedTab(Tab newTab) {
        if (mTab != null) mTab.removeObserver(TabSupplierObserver.this);
        mTab = newTab;
        if (mTab != null) mTab.addObserver(TabSupplierObserver.this);
    }

    /**
     * A notification that the observer has switched to observing a different tab.
     * @param tab The tab that the observer is now observing. This can be null.
     */
    protected void onObservingDifferentTab(Tab tab) {}

    /** Clean up any state held by this observer. */
    @Override
    @CallSuper
    public void destroy() {
        if (mTab != null) {
            mTab.removeObserver(this);
            mTab = null;
        }
        removeObserverFromTabSupplier();
    }

    @VisibleForTesting
    protected void updateObservedTabToCurrent() {
        updateObservedTab(mTabSupplier.get());
    }

    @VisibleForTesting
    protected void addObserverToTabSupplier() {
        mTabSupplier.addObserver(mTabObserver);
    }

    @VisibleForTesting
    protected void removeObserverFromTabSupplier() {
        mTabSupplier.removeObserver(mTabObserver);
    }
}
