// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import android.os.Bundle;

import androidx.annotation.ColorInt;
import androidx.annotation.Nullable;

import org.chromium.base.Token;
import org.chromium.build.annotations.NullMarked;

import java.util.ArrayList;
import java.util.Objects;

/**
 * Object that contains the metadata of a tab group, used for transferring tab group data during tab
 * group drag drop.
 */
@NullMarked
public class TabGroupMetadata {
    private static final String KEY_ROOT_ID = "rootId";
    private static final String KEY_SELECTED_TAB_ID = "selectedTabId";
    private static final String KEY_SOURCE_WINDOW_ID = "sourceWindowId";
    private static final String KEY_TAB_GROUP_ID = "tabGroupId";
    private static final String KEY_TAB_IDS = "tabIds";
    private static final String KEY_TAB_URLS = "tabUrls";
    private static final String KEY_TAB_GROUP_COLOR = "tabGroupColor";
    private static final String KEY_TAB_GROUP_TITLE = "tabGroupTitle";
    private static final String KEY_TAB_GROUP_COLLAPSED = "tabGroupCollapsed";
    private static final String KEY_IS_INCOGNITO = "isIncognito";

    public final int rootId;
    public final int selectedTabId;
    public final int sourceWindowId;
    public final Token tabGroupId;
    public final ArrayList<Integer> tabIds;
    public final ArrayList<String> tabUrls;
    public final @ColorInt int tabGroupColor;
    @Nullable public final String tabGroupTitle;
    public final boolean tabGroupCollapsed;
    public final boolean isIncognito;

    /**
     * Constructs a {@link TabGroupMetadata} object that stores metadata about a tab group.
     *
     * @param rootId The root ID of the group.
     * @param selectedTabId The selected tab ID of the group.
     * @param sourceWindowId The ID of the window that holds the tab group before re-parenting.
     * @param tabGroupId The stable ID for the tab group.
     * @param tabIds The list of tab IDs belonging to the group.
     * @param tabUrls The list of tab URLs belonging to the group.
     * @param tabGroupColor The color of the tab group.
     * @param tabGroupTitle The title of the tab group.
     * @param tabGroupCollapsed Whether the tab group is currently collapsed.
     * @param isIncognito Whether the tab group is in incognito mode.
     */
    public TabGroupMetadata(
            int rootId,
            int selectedTabId,
            int sourceWindowId,
            Token tabGroupId,
            ArrayList<Integer> tabIds,
            ArrayList<String> tabUrls,
            @ColorInt int tabGroupColor,
            @Nullable String tabGroupTitle,
            boolean tabGroupCollapsed,
            boolean isIncognito) {
        this.rootId = rootId;
        this.selectedTabId = selectedTabId;
        this.sourceWindowId = sourceWindowId;
        this.tabGroupId = tabGroupId;
        this.tabIds = tabIds;
        this.tabUrls = tabUrls;
        this.tabGroupColor = tabGroupColor;
        this.tabGroupTitle = tabGroupTitle;
        this.tabGroupCollapsed = tabGroupCollapsed;
        this.isIncognito = isIncognito;
    }

    /**
     * Converts this TabGroupMetadata into a bundle {@link Bundle}.
     *
     * @return the bundle {@link Bundle} stores the TabGroupMetadata properties.
     */
    public Bundle toBundle() {
        Bundle bundle = new Bundle();
        bundle.putInt(KEY_ROOT_ID, rootId);
        bundle.putInt(KEY_SELECTED_TAB_ID, selectedTabId);
        bundle.putInt(KEY_SOURCE_WINDOW_ID, sourceWindowId);
        bundle.putBundle(KEY_TAB_GROUP_ID, tabGroupId.toBundle());
        bundle.putIntegerArrayList(KEY_TAB_IDS, tabIds);
        bundle.putStringArrayList(KEY_TAB_URLS, tabUrls);
        bundle.putInt(KEY_TAB_GROUP_COLOR, tabGroupColor);
        bundle.putString(KEY_TAB_GROUP_TITLE, tabGroupTitle);
        bundle.putBoolean(KEY_TAB_GROUP_COLLAPSED, tabGroupCollapsed);
        bundle.putBoolean(KEY_IS_INCOGNITO, isIncognito);
        return bundle;
    }

    /**
     * @param bundle Bundle to be parsed.
     * @return the deserialized TabGroupMetadata object or null if the bundle is invalid.
     */
    public static @Nullable TabGroupMetadata maybeCreateFromBundle(@Nullable Bundle bundle) {
        if (bundle == null) return null;

        // A valid bundle should have all required properties.
        @Nullable
        Token tabGroupIdFromBundle =
                Token.maybeCreateFromBundle(bundle.getBundle(KEY_TAB_GROUP_ID));
        if (tabGroupIdFromBundle == null
                || bundle.getIntegerArrayList(KEY_TAB_IDS) == null
                || bundle.getStringArrayList(KEY_TAB_URLS) == null
                || bundle.getString(KEY_TAB_GROUP_TITLE) == null
                || !bundle.containsKey(KEY_ROOT_ID)
                || !bundle.containsKey(KEY_SELECTED_TAB_ID)
                || !bundle.containsKey(KEY_SOURCE_WINDOW_ID)
                || !bundle.containsKey(KEY_TAB_GROUP_COLOR)
                || !bundle.containsKey(KEY_TAB_GROUP_COLLAPSED)
                || !bundle.containsKey(KEY_IS_INCOGNITO)) return null;

        TabGroupMetadata tabGroupMetadata =
                new TabGroupMetadata(
                        bundle.getInt(KEY_ROOT_ID),
                        bundle.getInt(KEY_SELECTED_TAB_ID),
                        bundle.getInt(KEY_SOURCE_WINDOW_ID),
                        tabGroupIdFromBundle,
                        bundle.getIntegerArrayList(KEY_TAB_IDS),
                        bundle.getStringArrayList(KEY_TAB_URLS),
                        bundle.getInt(KEY_TAB_GROUP_COLOR),
                        bundle.getString(KEY_TAB_GROUP_TITLE),
                        bundle.getBoolean(KEY_TAB_GROUP_COLLAPSED),
                        bundle.getBoolean(KEY_IS_INCOGNITO));
        return tabGroupMetadata;
    }

    @Override
    public boolean equals(Object other) {
        if (this == other) return true;
        if (other == null || getClass() != other.getClass()) return false;
        TabGroupMetadata that = (TabGroupMetadata) other;
        return rootId == that.rootId
                && selectedTabId == that.selectedTabId
                && sourceWindowId == that.sourceWindowId
                && tabGroupColor == that.tabGroupColor
                && tabGroupCollapsed == that.tabGroupCollapsed
                && isIncognito == that.isIncognito
                && Objects.equals(tabGroupId, that.tabGroupId)
                && Objects.equals(tabIds, that.tabIds)
                && Objects.equals(tabUrls, that.tabUrls)
                && Objects.equals(tabGroupTitle, that.tabGroupTitle);
    }

    @Override
    public int hashCode() {
        return Objects.hash(
                this.rootId,
                this.selectedTabId,
                this.sourceWindowId,
                this.tabGroupId,
                this.tabIds,
                this.tabUrls,
                this.tabGroupColor,
                this.tabGroupTitle,
                this.tabGroupCollapsed,
                this.isIncognito);
    }

    public String toDebugString() {
        return "TabGroupMetadata{"
                + "rootId="
                + rootId
                + "selectedTabId="
                + selectedTabId
                + "sourceWindowId="
                + sourceWindowId
                + ", tabGroupId="
                + tabGroupId
                + ", tabIds="
                + tabIds
                + ", tabUrls="
                + tabUrls
                + ", tabGroupColor="
                + tabGroupColor
                + ", tabGroupTitle='"
                + tabGroupTitle
                + '\''
                + ", isCollapsed="
                + tabGroupCollapsed
                + ", isIncognito="
                + isIncognito
                + '}';
    }
}
