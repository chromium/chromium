// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_groups;

import android.content.Context;
import android.content.SharedPreferences;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.tab.Tab;

/**
 * Helper class to handle persistence of the expanded/collapsed state of a tab group. Default is
 * expanded unless explicitly set.
 */
class TabGroupCollapsedUtils {
    private static final String TAB_GROUP_COLLAPSED_FILE_NAME = "tab_group_collapsed";

    /**
     * @param tabRootId The tab root ID whose related group's collapsed state will be stored.
     * @param isCollapsed If the tab group is collapsed or expanded.
     */
    static void storeTabGroupCollapsed(int tabRootId, boolean isCollapsed) {
        assert tabRootId != Tab.INVALID_TAB_ID;
        if (isCollapsed) {
            getSharedPreferences().edit().putBoolean(String.valueOf(tabRootId), true).apply();
        } else {
            deleteTabGroupCollapsed(tabRootId);
        }
    }

    /**
     * @param tabRootId The tab root ID whose related tab group collapsed state will be deleted.
     */
    static void deleteTabGroupCollapsed(int tabRootId) {
        assert tabRootId != Tab.INVALID_TAB_ID;
        getSharedPreferences().edit().remove(String.valueOf(tabRootId)).apply();
    }

    /**
     * @param tabRootId The tab root ID whose related group's collapsed state will be fetched.
     * @return Whether the tab group is collapsed or expanded.
     */
    static boolean getTabGroupCollapsed(int tabRootId) {
        assert tabRootId != Tab.INVALID_TAB_ID;
        return getSharedPreferences().getBoolean(String.valueOf(tabRootId), false);
    }

    private static SharedPreferences getSharedPreferences() {
        return ContextUtils.getApplicationContext()
                .getSharedPreferences(TAB_GROUP_COLLAPSED_FILE_NAME, Context.MODE_PRIVATE);
    }
}
