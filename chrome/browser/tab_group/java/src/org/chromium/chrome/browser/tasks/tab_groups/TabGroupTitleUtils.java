// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_groups;

import android.content.Context;
import android.content.SharedPreferences;

import androidx.annotation.Nullable;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.tab.Tab;

/** Helper class to handle tab group title related utilities. */
public class TabGroupTitleUtils {
    private static final String TAB_GROUP_TITLES_FILE_NAME = "tab_group_titles";

    /**
     * This method stores tab group title with reference to {@code tabRootId}.
     * @param tabRootId   The tab root ID which is used as reference to store group title.
     * @param title       The tab group title to store.
     */
    public static void storeTabGroupTitle(int tabRootId, String title) {
        assert tabRootId != Tab.INVALID_TAB_ID;
        getSharedPreferences().edit().putString(String.valueOf(tabRootId), title).apply();
    }

    /**
     * This method deletes specific stored tab group title with reference to {@code tabRootId}.
     * @param tabRootId  The tab root ID whose related tab group title will be deleted.
     */
    // Package Private.
    public static void deleteTabGroupTitle(int tabRootId) {
        assert tabRootId != Tab.INVALID_TAB_ID;
        getSharedPreferences().edit().remove(String.valueOf(tabRootId)).apply();
    }

    /**
     * This method fetches tab group title with related tab group root ID.
     * @param tabRootId  The tab root ID whose related tab group title will be fetched.
     * @return The stored title of the target tab group, default value is null.
     */
    public static @Nullable String getTabGroupTitle(int tabRootId) {
        assert tabRootId != Tab.INVALID_TAB_ID;
        // TODO(crbug/1419842): Consider checking if this looks like the default plural string and
        // deleting and returning null if any users have saved tab group titles.
        return getSharedPreferences().getString(String.valueOf(tabRootId), null);
    }

    private static SharedPreferences getSharedPreferences() {
        return ContextUtils.getApplicationContext()
                .getSharedPreferences(TAB_GROUP_TITLES_FILE_NAME, Context.MODE_PRIVATE);
    }
}
