// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import android.os.Bundle;

import androidx.annotation.ColorInt;
import androidx.annotation.Nullable;

import org.chromium.base.Token;
import org.chromium.build.annotations.NullMarked;

import java.util.HashMap;
import java.util.LinkedHashMap;
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
    private static final String KEY_TAB_IDS_TO_URLS = "tabIdsToUrls";
    private static final String KEY_TAB_GROUP_COLOR = "tabGroupColor";
    private static final String KEY_TAB_GROUP_TITLE = "tabGroupTitle";
    private static final String KEY_TAB_GROUP_COLLAPSED = "tabGroupCollapsed";
    private static final String KEY_IS_GROUP_SHARED = "isGroupShared";
    private static final String KEY_IS_INCOGNITO = "isIncognito";
    private static final String KEY_MHTML_TAB_TITLE = "mhtmlTabTitle";

    public final int rootId;
    public final int selectedTabId;
    public final int sourceWindowId;
    public final Token tabGroupId;
    public final LinkedHashMap<Integer, String> tabIdsToUrls;
    public final @ColorInt int tabGroupColor;
    @Nullable public final String tabGroupTitle;
    @Nullable public final String mhtmlTabTitle;
    public final boolean tabGroupCollapsed;
    public final boolean isGroupShared;
    public final boolean isIncognito;

    /**
     * Constructs a {@link TabGroupMetadata} object that stores metadata about a tab group.
     *
     * @param rootId The root ID of the group.
     * @param selectedTabId The selected tab ID of the group.
     * @param sourceWindowId The ID of the window that holds the tab group before re-parenting.
     * @param tabGroupId The stable ID for the tab group.
     * @param tabIdsToUrls The LinkedHashMap containing key-value pairs of tab IDs and URLs.
     * @param tabGroupColor The color of the tab group.
     * @param tabGroupTitle The title of the tab group.
     * @param mhtmlTabTitle The title of the first MHTML tab in the group if there is any.
     * @param tabGroupCollapsed Whether the tab group is currently collapsed.
     * @param isGroupShared Whether the tab group is shared with other collaborators.
     * @param isIncognito Whether the tab group is in incognito mode.
     */
    public TabGroupMetadata(
            int rootId,
            int selectedTabId,
            int sourceWindowId,
            Token tabGroupId,
            LinkedHashMap<Integer, String> tabIdsToUrls,
            @ColorInt int tabGroupColor,
            @Nullable String tabGroupTitle,
            @Nullable String mhtmlTabTitle,
            boolean tabGroupCollapsed,
            boolean isGroupShared,
            boolean isIncognito) {
        this.rootId = rootId;
        this.selectedTabId = selectedTabId;
        this.sourceWindowId = sourceWindowId;
        this.tabGroupId = tabGroupId;
        this.tabIdsToUrls = tabIdsToUrls;
        this.tabGroupColor = tabGroupColor;
        this.tabGroupTitle = tabGroupTitle;
        this.mhtmlTabTitle = mhtmlTabTitle;
        this.tabGroupCollapsed = tabGroupCollapsed;
        this.isGroupShared = isGroupShared;
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
        bundle.putSerializable(KEY_TAB_IDS_TO_URLS, tabIdsToUrls);
        bundle.putInt(KEY_TAB_GROUP_COLOR, tabGroupColor);
        bundle.putString(KEY_TAB_GROUP_TITLE, tabGroupTitle);
        bundle.putString(KEY_MHTML_TAB_TITLE, mhtmlTabTitle);
        bundle.putBoolean(KEY_TAB_GROUP_COLLAPSED, tabGroupCollapsed);
        bundle.putBoolean(KEY_IS_GROUP_SHARED, isGroupShared);
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
        LinkedHashMap<Integer, String> tabIdsToUrls =
                new LinkedHashMap<>(
                        (HashMap<Integer, String>) bundle.getSerializable(KEY_TAB_IDS_TO_URLS));
        if (tabGroupIdFromBundle == null
                || tabIdsToUrls == null
                || tabIdsToUrls.isEmpty()
                || !bundle.containsKey(KEY_ROOT_ID)
                || !bundle.containsKey(KEY_SELECTED_TAB_ID)
                || !bundle.containsKey(KEY_SOURCE_WINDOW_ID)
                || !bundle.containsKey(KEY_TAB_GROUP_COLOR)
                || !bundle.containsKey(KEY_TAB_GROUP_COLLAPSED)
                || !bundle.containsKey(KEY_IS_GROUP_SHARED)
                || !bundle.containsKey(KEY_IS_INCOGNITO)) return null;

        TabGroupMetadata tabGroupMetadata =
                new TabGroupMetadata(
                        bundle.getInt(KEY_ROOT_ID),
                        bundle.getInt(KEY_SELECTED_TAB_ID),
                        bundle.getInt(KEY_SOURCE_WINDOW_ID),
                        tabGroupIdFromBundle,
                        tabIdsToUrls,
                        bundle.getInt(KEY_TAB_GROUP_COLOR),
                        bundle.getString(KEY_TAB_GROUP_TITLE),
                        bundle.getString(KEY_MHTML_TAB_TITLE),
                        bundle.getBoolean(KEY_TAB_GROUP_COLLAPSED),
                        bundle.getBoolean(KEY_IS_GROUP_SHARED),
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
                && isGroupShared == that.isGroupShared
                && isIncognito == that.isIncognito
                && Objects.equals(tabGroupId, that.tabGroupId)
                && Objects.equals(tabIdsToUrls, that.tabIdsToUrls)
                && Objects.equals(tabGroupTitle, that.tabGroupTitle)
                && Objects.equals(mhtmlTabTitle, that.mhtmlTabTitle);
    }

    @Override
    public int hashCode() {
        return Objects.hash(
                this.rootId,
                this.selectedTabId,
                this.sourceWindowId,
                this.tabGroupId,
                this.tabIdsToUrls,
                this.tabGroupColor,
                this.tabGroupTitle,
                this.mhtmlTabTitle,
                this.tabGroupCollapsed,
                this.isGroupShared,
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
                + ", tabIdsToUrls="
                + tabIdsToUrls
                + ", tabGroupColor="
                + tabGroupColor
                + ", tabGroupTitle='"
                + tabGroupTitle
                + '\''
                + ", mhtmlTabTitle='"
                + mhtmlTabTitle
                + '\''
                + ", isCollapsed="
                + tabGroupCollapsed
                + ", isGroupShared="
                + isGroupShared
                + ", isIncognito="
                + isIncognito
                + '}';
    }
}
