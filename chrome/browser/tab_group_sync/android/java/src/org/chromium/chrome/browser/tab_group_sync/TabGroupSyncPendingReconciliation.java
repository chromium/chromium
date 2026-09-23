// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_group_sync;

import org.chromium.base.UserData;
import org.chromium.base.UserDataHost;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabId;

/**
 * A {@link UserData} marker attached to a {@link Tab} indicating that the tab requires pending
 * reconciliation with {@link org.chromium.components.tab_group_sync.TabGroupSyncService}. Records
 * any replaced local tab ID so that tab group synchronization reconciles the local tab ID mapping
 * and commits the tab's resting URL without clobbering local state or emitting duplicate tabs.
 */
@NullMarked
public final class TabGroupSyncPendingReconciliation implements UserData {
    private static final Class<TabGroupSyncPendingReconciliation> USER_DATA_KEY =
            TabGroupSyncPendingReconciliation.class;

    private final @TabId int mReplacedLocalTabId;

    private TabGroupSyncPendingReconciliation(@TabId int replacedLocalTabId) {
        mReplacedLocalTabId = replacedLocalTabId;
    }

    /**
     * Marks the given tab as requiring pending reconciliation with sync by attaching a {@link
     * TabGroupSyncPendingReconciliation} instance to the tab's {@link UserDataHost}. Callers must
     * ensure that pending reconciliation has not already been attached to the tab.
     *
     * @param tab The tab requiring pending reconciliation of its replaced ID and resting URL.
     * @param replacedLocalTabId The local ID of the replaced tab, or {@link Tab#INVALID_TAB_ID}.
     * @return The attached reconciliation marker instance, or null if the tab is null/destroyed.
     */
    public static @Nullable TabGroupSyncPendingReconciliation suppress(
            @Nullable Tab tab, @TabId int replacedLocalTabId) {
        if (tab == null || tab.isDestroyed()) return null;
        UserDataHost host = tab.getUserDataHost();
        if (host == null) return null;
        assert host.getUserData(USER_DATA_KEY) == null
                : "TabGroupSyncPendingReconciliation already set";
        return host.setUserData(
                USER_DATA_KEY, new TabGroupSyncPendingReconciliation(replacedLocalTabId));
    }

    /**
     * Clears any pending reconciliation marker for the given tab once its local tab ID and resting
     * URL have been reconciled with sync.
     *
     * @param tab The tab on which to clear pending reconciliation.
     */
    public static void clear(@Nullable Tab tab) {
        if (tab == null || tab.isDestroyed()) return;
        UserDataHost host = tab.getUserDataHost();
        if (host != null && host.getUserData(USER_DATA_KEY) != null) {
            host.removeUserData(USER_DATA_KEY);
        }
    }

    /**
     * Returns whether the given tab currently has pending reconciliation of its replaced tab ID or
     * resting URL.
     *
     * @param tab The tab to check.
     * @return True if pending reconciliation is active for the tab, false otherwise.
     */
    public static boolean isSuppressed(@Nullable Tab tab) {
        return from(tab) != null;
    }

    /**
     * Resolves the effective root local tab ID being replaced when swapping {@code replacedTab}. If
     * {@code replacedTab} itself still carries an unreconciled replaced tab ID from an earlier
     * swap, returns that prior replaced ID so chained swaps preserve the root synced {@code
     * localId}.
     *
     * @param replacedTab The tab being replaced in a swap.
     * @return The effective local tab ID to reconcile in sync, or {@link Tab#INVALID_TAB_ID}.
     */
    public static @TabId int resolveEffectiveReplacedTabId(@Nullable Tab replacedTab) {
        if (replacedTab == null || replacedTab.isDestroyed()) return Tab.INVALID_TAB_ID;
        @TabId int priorReplacedId = getReplacedLocalTabId(replacedTab);
        return priorReplacedId != Tab.INVALID_TAB_ID ? priorReplacedId : replacedTab.getId();
    }

    @Override
    public void destroy() {}

    /**
     * Returns the {@link TabGroupSyncPendingReconciliation} instance attached to the tab, if any.
     *
     * @param tab The tab to query.
     * @return The reconciliation marker instance or null.
     */
    static @Nullable TabGroupSyncPendingReconciliation from(@Nullable Tab tab) {
        if (tab == null || tab.isDestroyed()) return null;
        UserDataHost host = tab.getUserDataHost();
        return host != null ? host.getUserData(USER_DATA_KEY) : null;
    }

    static @TabId int getReplacedLocalTabId(@Nullable Tab tab) {
        TabGroupSyncPendingReconciliation reconciliation = from(tab);
        return reconciliation != null ? reconciliation.mReplacedLocalTabId : Tab.INVALID_TAB_ID;
    }
}
