// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_ui;

import android.graphics.drawable.Drawable;
import android.util.Size;

import org.chromium.base.Callback;
import org.chromium.base.Token;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.url.GURL;

import java.util.Collections;
import java.util.List;
import java.util.Objects;

/** An interface to get the thumbnails to be shown inside the tab grid cards. */
@NullMarked
public interface ThumbnailProvider {
    /**
     * The metadata details for the multi thumbnail view representing tabs and group cards. This
     * object sources data from both real {@link Tab}s/tab groups and {@code SavedTabGroup}s. If
     * both {@link #tabId} is {@link Tab#INVALID_TAB_ID} and {@link #tabGroupId} is null, a {@code
     * SavedTabGroup} is being referenced via {@link #urlList}.
     */
    class MultiThumbnailMetadata {
        public final int tabId;
        public final @Nullable Token tabGroupId;
        public final List<GURL> urlList;
        public final boolean isIncognito;
        public final @Nullable @TabGroupColorId Integer tabGroupColor;
        public final List<Integer> actingTabIds;

        private MultiThumbnailMetadata(
                int tabId,
                @Nullable Token tabGroupId,
                List<GURL> urlList,
                boolean isIncognito,
                @Nullable @TabGroupColorId Integer tabGroupColor,
                List<Integer> actingTabIds) {
            this.tabId = tabId;
            this.tabGroupId = tabGroupId;
            this.urlList = urlList;
            this.isIncognito = isIncognito;
            this.tabGroupColor = tabGroupColor;
            this.actingTabIds = actingTabIds;
        }

        /**
         * Creates a {@link MultiThumbnailMetadata} object for a {@code SavedTabGroup}.
         *
         * @param urlList The list of URLs of the tabs in the saved tab group.
         * @param isIncognito Whether the saved tab group is in incognito mode.
         * @param tabGroupColor The color ID of the saved tab group.
         * @return The {@link MultiThumbnailMetadata} for the saved tab group.
         */
        public static MultiThumbnailMetadata createMetadataForSavedTabGroup(
                List<GURL> urlList, boolean isIncognito, @TabGroupColorId int tabGroupColor) {
            return new MultiThumbnailMetadata(
                    Tab.INVALID_TAB_ID,
                    /* tabGroupId= */ null,
                    urlList,
                    isIncognito,
                    tabGroupColor,
                    /* actingTabIds= */ Collections.emptyList());
        }

        /**
         * Creates a {@link MultiThumbnailMetadata} object for a single tab.
         *
         * @param tabId The ID of the tab.
         * @return The {@link MultiThumbnailMetadata} for the single tab.
         */
        public static MultiThumbnailMetadata createMetadataForSingleTab(int tabId) {
            return new MultiThumbnailMetadata(
                    tabId,
                    /* tabGroupId= */ null,
                    /* urlList= */ Collections.emptyList(),
                    /* isIncognito= */ false,
                    /* tabGroupColor= */ null,
                    /* actingTabIds= */ Collections.emptyList());
        }

        /**
         * Creates a {@link MultiThumbnailMetadata} object for a tab group.
         *
         * @param tabGroupId The {@link Token} ID of the tab group.
         * @param isIncognito Whether the tab group is in incognito mode.
         * @param tabGroupColor The color ID of the tab group.
         * @param actingTabIds The list of tab IDs currently acting in the group.
         * @return The {@link MultiThumbnailMetadata} for the tab group.
         */
        public static MultiThumbnailMetadata createMetadataForTabGroup(
                Token tabGroupId,
                boolean isIncognito,
                @TabGroupColorId int tabGroupColor,
                List<Integer> actingTabIds) {
            Collections.sort(actingTabIds);
            return new MultiThumbnailMetadata(
                    Tab.INVALID_TAB_ID,
                    tabGroupId,
                    /* urlList= */ Collections.emptyList(),
                    isIncognito,
                    tabGroupColor,
                    actingTabIds);
        }

        @Override
        public int hashCode() {
            return Objects.hash(
                    this.tabId,
                    this.tabGroupId,
                    this.urlList,
                    this.isIncognito,
                    this.tabGroupColor,
                    this.actingTabIds);
        }

        @Override
        public boolean equals(Object obj) {
            return (obj instanceof MultiThumbnailMetadata other)
                    && this.tabId == other.tabId
                    && Objects.equals(this.tabGroupId, other.tabGroupId)
                    && Objects.equals(this.urlList, other.urlList)
                    && this.isIncognito == other.isIncognito
                    && Objects.equals(this.tabGroupColor, other.tabGroupColor)
                    && Objects.equals(this.actingTabIds, other.actingTabIds);
        }
    }

    /**
     * Fetches a tab thumbnail in the form of a drawable. Usually from {@link TabContentManager}.
     *
     * @param metadata The metadata of the tab or group to fetch the thumbnail of.
     * @param thumbnailSize The size of the thumbnail to retrieve.
     * @param isSelected Whether the tab is currently selected. Ignored if not multi-thumbnail.
     * @param callback Uses a {@link Drawable} instead of a {@link Bitmap} for flexibility. May
     *     receive null if no bitmap is returned.
     */
    void getTabThumbnailWithCallback(
            MultiThumbnailMetadata metadata,
            Size thumbnailSize,
            boolean isSelected,
            Callback<@Nullable Drawable> callback);
}
