// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_groups;

import android.content.Context;
import android.content.SharedPreferences;
import android.text.TextUtils;

import androidx.annotation.Nullable;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.tab.Tab;

import java.util.Objects;

/** Helper class to handle tab group title related utilities. */
public class TabGroupTitleUtils {
    private static final String TAB_GROUP_TITLES_FILE_NAME = "tab_group_titles";

    /**
     * @param context Context for accessing resources.
     * @param numRelatedTabs The number of related tabs.
     * @return the default title for the tab group.
     */
    public static String getDefaultTitle(Context context, int numRelatedTabs) {
        return context.getResources()
                .getQuantityString(
                        R.plurals.bottom_tab_grid_title_placeholder,
                        numRelatedTabs,
                        numRelatedTabs);
    }

    /**
     * @param context To load resources from.
     * @param newTitle the new title.
     * @param numRelatedTabs the number of related tabs in the group.
     * @return whether the newTitle is a match for the default string.
     */
    public static boolean isDefaultTitle(Context context, String newTitle, int numRelatedTabs) {
        // TODO(crbug.com/40895368): Consider broadening this check for differing numbers of related
        // tabs. This is difficult due to this being a translated plural string.
        String defaultTitle = getDefaultTitle(context, numRelatedTabs);
        return Objects.equals(newTitle, defaultTitle);
    }

    /**
     * Returns a displayable title for a given tab group by root id. While this function can help
     * some UI surfaces, sometimes it is difficult to follow MVC with this approach.
     *
     * @param context To load resources from.
     * @param tabGroupModelFilter To read tab and tab group data from.
     * @param rootId The identifying id of the tab group.
     * @return A non-null string that can be shown to users.
     */
    public static String getDisplayableTitle(
            Context context, TabGroupModelFilter tabGroupModelFilter, int rootId) {
        @Nullable String explicitTitle = tabGroupModelFilter.getTabGroupTitle(rootId);
        if (TextUtils.isEmpty(explicitTitle)) {
            int tabCount = tabGroupModelFilter.getRelatedTabCountForRootId(rootId);
            return getDefaultTitle(context, tabCount);
        } else {
            return explicitTitle;
        }
    }

    /**
     * This method stores tab group title with reference to {@code tabRootId}. Package protected as
     * all access should route through the {@link TabGroupModelFilter}.
     *
     * @param tabRootId The tab root ID which is used as reference to store group title.
     * @param title The tab group title to store.
     */
    static void storeTabGroupTitle(int tabRootId, String title) {
        assert tabRootId != Tab.INVALID_TAB_ID;
        if (TextUtils.isEmpty(title)) {
            deleteTabGroupTitle(tabRootId);
        } else {
            getSharedPreferences().edit().putString(String.valueOf(tabRootId), title).apply();
        }
    }

    /**
     * This method deletes specific stored tab group title with reference to {@code tabRootId}.
     * While currently public, the intent is to make this package protected and force all access to
     * go through the {@Link TabGroupModelFilter}.
     *
     * @param tabRootId The tab root ID whose related tab group title will be deleted.
     */
    static void deleteTabGroupTitle(int tabRootId) {
        assert tabRootId != Tab.INVALID_TAB_ID;
        getSharedPreferences().edit().remove(String.valueOf(tabRootId)).apply();
    }

    /**
     * This method fetches tab group title with related tab group root ID. While currently public,
     * the intent is to make this package protected and force all access to go through the {@Link
     * TabGroupModelFilter}.
     *
     * @param tabRootId The tab root ID whose related tab group title will be fetched.
     * @return The stored title of the target tab group, default value is null.
     */
    static @Nullable String getTabGroupTitle(int tabRootId) {
        assert tabRootId != Tab.INVALID_TAB_ID;
        // TODO(crbug.com/40895368): Consider checking if this looks like the default plural string
        // and
        // deleting and returning null if any users have saved tab group titles.
        return getSharedPreferences().getString(String.valueOf(tabRootId), null);
    }

    private static SharedPreferences getSharedPreferences() {
        return ContextUtils.getApplicationContext()
                .getSharedPreferences(TAB_GROUP_TITLES_FILE_NAME, Context.MODE_PRIVATE);
    }
}
