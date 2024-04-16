// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_group_sync;

import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.SavedTabGroupTab;
import org.chromium.url.GURL;

/** Test utils for tab group sync. */
public class TabGroupSyncTestUtils {
    /** Create a test saved tab group. */
    public static SavedTabGroup createSavedTabGroup() {
        SavedTabGroup group = new SavedTabGroup();
        group.syncId = "Group_1";
        group.title = "Group 1";
        group.color = org.chromium.components.tab_groups.TabGroupColorId.GREEN;
        SavedTabGroupTab tab1 =
                createSavedTabGroupTab("Tab_1", group.syncId, "Tab 1", "https://foo1.com", 0);
        group.savedTabs.add(tab1);

        SavedTabGroupTab tab2 =
                createSavedTabGroupTab("Tab_2", group.syncId, "Tab 2", "https://foo2.com", 1);
        group.savedTabs.add(tab2);
        return group;
    }

    private static SavedTabGroupTab createSavedTabGroupTab(
            String syncId, String syncGroupId, String title, String url, int position) {
        SavedTabGroupTab tab = new SavedTabGroupTab();
        tab.syncId = syncId;
        tab.syncGroupId = syncGroupId;
        tab.title = title;
        tab.url = new GURL(url);
        tab.position = position;
        return tab;
    }
}
