// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_group_sync;

import org.chromium.base.Token;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.components.tab_group_sync.LocalTabGroupId;

/** Utility methods for tab group sync. */
public final class TabGroupSyncUtils {

    /**
     * Whether the given {@param localId} corresponds to a tab group in the current window
     * corresponding to {@param tabGroupModelFilter}.
     *
     * @param tabGroupModelFilter The tab group model filter in which to find the tab group.
     * @param localId The ID of the tab group.
     */
    public static boolean isInCurrentWindow(
            TabGroupModelFilter tabGroupModelFilter, LocalTabGroupId localId) {
        int rootId = tabGroupModelFilter.getRootIdFromStableId(localId.tabGroupId);
        return rootId != Tab.INVALID_TAB_ID;
    }

    /** Conversion method to get a {@link LocalTabGroupId} from a root ID. */
    public static LocalTabGroupId getLocalTabGroupId(TabGroupModelFilter filter, int rootId) {
        Token tabGroupId = filter.getStableIdFromRootId(rootId);
        return tabGroupId == null ? null : new LocalTabGroupId(tabGroupId);
    }

    /** Conversion method to get a root ID from a {@link LocalTabGroupId}. */
    public static int getRootId(TabGroupModelFilter filter, LocalTabGroupId localTabGroupId) {
        assert localTabGroupId != null;
        return filter.getRootIdFromStableId(localTabGroupId.tabGroupId);
    }

    /** Util method to get a {@link LocalTabGroupId} from a tab. */
    public static LocalTabGroupId getLocalTabGroupId(Tab tab) {
        return new LocalTabGroupId(tab.getTabGroupId());
    }
}
