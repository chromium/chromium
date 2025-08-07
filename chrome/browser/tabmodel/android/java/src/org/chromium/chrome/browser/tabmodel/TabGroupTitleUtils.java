// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.text.TextUtils;

import org.chromium.base.Token;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;

import java.util.List;
import java.util.Objects;

/** Helper class to handle tab group title related utilities. */
@NullMarked
public class TabGroupTitleUtils {

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
     * @param tabGroupId The identifying tab group id of the tab group.
     * @return A non-null string that can be shown to users.
     */
    public static String getDisplayableTitle(
            Context context, TabGroupModelFilter tabGroupModelFilter, @Nullable Token tabGroupId) {
        boolean tabGroupExists =
                tabGroupId != null && tabGroupModelFilter.tabGroupExists(tabGroupId);
        String explicitTitle =
                tabGroupExists
                        ? tabGroupModelFilter.getTabGroupTitle(assumeNonNull(tabGroupId))
                        : null;
        if (TextUtils.isEmpty(explicitTitle)) {
            int tabCount = 0;
            List<Tab> tabsInGroup = tabGroupModelFilter.getTabsInGroup(assumeNonNull(tabGroupId));
            for (Tab tab : tabsInGroup) {
                if (!tab.isClosing()) {
                    tabCount++;
                }
            }
            return getDefaultTitle(context, tabCount);
        } else {
            return explicitTitle;
        }
    }
}
