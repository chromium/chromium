// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.text.TextUtils;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * This class hosts logic related to edit tab group title. Concrete class that extends this abstract
 * class needs to specify the title storage/fetching implementation as well as handle {@link
 * PropertyModel} update.
 */
public abstract class TabGroupTitleEditor {
    private final Context mContext;

    public TabGroupTitleEditor(Context context) {
        mContext = context;
    }

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
     * @param newTitle the new title.
     * @param numRelatedTabs the number of related tabs in the group.
     * @return whether the newTitle is a match for the default string.
     */
    public boolean isDefaultTitle(String newTitle, int numRelatedTabs) {
        // TODO(crbug.com/40895368): Consider broadening this check for differing numbers of related
        // tabs. This is difficult due to this being a translated plural string.
        return newTitle.equals(getDefaultTitle(mContext, numRelatedTabs));
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
     * This method uses {@code title} to update the {@link PropertyModel} of the group which
     * contains {@code tab}. Concrete class need to specify how to update title in {@link
     * PropertyModel}.
     *
     * @param tab The {@link Tab} whose relevant tab group's title will be updated.
     * @param title The tab group title to update.
     */
    protected abstract void updateTabGroupTitle(Tab tab, String title);

    /**
     * This method uses tab group root ID as a reference to store tab group title.
     *
     * @param tabRootId    The tab root ID of the tab group whose title will be stored.
     * @param title        The tab group title to store.
     */
    protected abstract void storeTabGroupTitle(int tabRootId, String title);

    /**
     * This method uses tab group root ID as a reference to delete specific stored tab group title.
     * @param tabRootId   The tab root ID whose related tab group title will be deleted.
     */
    protected abstract void deleteTabGroupTitle(int tabRootId);

    /**
     * This method uses tab group root ID to fetch stored tab group title.
     * @param tabRootId  The tab root ID whose related tab group title will be fetched.
     * @return The stored title of the related group.
     */
    protected abstract String getTabGroupTitle(int tabRootId);
}
