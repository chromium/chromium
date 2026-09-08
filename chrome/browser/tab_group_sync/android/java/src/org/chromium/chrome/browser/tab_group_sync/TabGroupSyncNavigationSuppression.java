// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_group_sync;

import org.chromium.base.UserData;
import org.chromium.base.UserDataHost;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;

/**
 * A {@link UserData} marker attached to a {@link Tab} to suppress sync-initiated
 * URL reconciliation navigations in {@link LocalTabGroupMutationHelper}.
 */
@NullMarked
public final class TabGroupSyncNavigationSuppression implements UserData {
    private static final Class<TabGroupSyncNavigationSuppression> USER_DATA_KEY =
            TabGroupSyncNavigationSuppression.class;

    private TabGroupSyncNavigationSuppression() {}

    /**
     * Suppresses sync navigation for the given tab by attaching a {@link
     * TabGroupSyncNavigationSuppression} instance to the tab's {@link UserDataHost}.
     * Callers must ensure that suppression has not already been created for the tab.
     *
     * @param tab The tab to suppress sync navigations on.
     * @return The attached suppression instance, or null if the tab is null/destroyed.
     */
    public static @Nullable TabGroupSyncNavigationSuppression suppress(@Nullable Tab tab) {
        if (tab == null || tab.isDestroyed()) return null;
        UserDataHost host = tab.getUserDataHost();
        if (host == null) return null;
        assert host.getUserData(USER_DATA_KEY) == null
                : "TabGroupSyncNavigationSuppression already set";
        return host.setUserData(USER_DATA_KEY, new TabGroupSyncNavigationSuppression());
    }

    /**
     * Returns whether sync navigation is currently suppressed for the given tab.
     *
     * @param tab The tab to check.
     * @return True if navigation is suppressed, false otherwise.
     */
    public static boolean isSuppressed(@Nullable Tab tab) {
        if (tab == null || tab.isDestroyed()) return false;
        UserDataHost host = tab.getUserDataHost();
        return host != null && host.getUserData(USER_DATA_KEY) != null;
    }

    /**
     * Clears any active navigation suppression for the given tab.
     *
     * @param tab The tab on which to clear navigation suppression.
     */
    public static void clear(@Nullable Tab tab) {
        if (tab == null || tab.isDestroyed()) return;
        UserDataHost host = tab.getUserDataHost();
        if (host != null && host.getUserData(USER_DATA_KEY) != null) {
            host.removeUserData(USER_DATA_KEY);
        }
    }

    /**
     * Returns the {@link TabGroupSyncNavigationSuppression} instance attached to the tab, if any.
     *
     * @param tab The tab to query.
     * @return The suppression instance or null.
     */
    public static @Nullable TabGroupSyncNavigationSuppression from(@Nullable Tab tab) {
        if (tab == null || tab.isDestroyed()) return null;
        UserDataHost host = tab.getUserDataHost();
        return host != null ? host.getUserData(USER_DATA_KEY) : null;
    }

    @Override
    public void destroy() {}
}
