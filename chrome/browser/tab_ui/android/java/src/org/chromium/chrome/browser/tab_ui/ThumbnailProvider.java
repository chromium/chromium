// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_ui;

import android.graphics.drawable.Drawable;
import android.util.Size;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
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
     * object sources data from both real {@link Tab}s that may be in groups and {@link
     * SavedTabGroup}s. If the tabId is an INVALID_TAB_ID, a SavedTabGroup is being referenced. The
     * urlList may be empty for non-SavedTabGroup groups as they will be parsed via the model filter
     * in {@link MultiThumbnailFetcher}.
     */
    class MultiThumbnailMetadata {
        public final int tabId;
        public final List<GURL> urlList;
        public final boolean isInTabGroup;
        public final boolean isIncognito;
        public final @Nullable @TabGroupColorId Integer tabGroupColor;

        private MultiThumbnailMetadata(
                int tabId,
                List<GURL> urlList,
                boolean isInTabGroup,
                boolean isIncognito,
                @Nullable @TabGroupColorId Integer tabGroupColor) {
            this.tabId = tabId;
            this.urlList = urlList;
            this.isInTabGroup = isInTabGroup;
            this.isIncognito = isIncognito;
            this.tabGroupColor = tabGroupColor;
        }

        /** Create a {@link MultiThumbnailMetadata} object with a urlList. */
        public static MultiThumbnailMetadata createMetadataWithUrls(
                int tabId,
                List<GURL> urlList,
                boolean isInTabGroup,
                boolean isIncognito,
                @Nullable @TabGroupColorId Integer tabGroupColor) {
            return new MultiThumbnailMetadata(
                    tabId, urlList, isInTabGroup, isIncognito, tabGroupColor);
        }

        /** Create a {@link MultiThumbnailMetadata} object without requiring a urlList. */
        public static MultiThumbnailMetadata createMetadataWithoutUrls(
                int tabId,
                boolean isInTabGroup,
                boolean isIncognito,
                @Nullable @TabGroupColorId Integer tabGroupColor) {
            return new MultiThumbnailMetadata(
                    tabId, Collections.emptyList(), isInTabGroup, isIncognito, tabGroupColor);
        }

        @Override
        public int hashCode() {
            return Objects.hash(
                    this.tabId,
                    this.urlList,
                    this.isInTabGroup,
                    this.isIncognito,
                    this.tabGroupColor);
        }

        @Override
        public boolean equals(Object obj) {
            return (obj instanceof MultiThumbnailMetadata other)
                    && this.tabId == other.tabId
                    && Objects.equals(this.urlList, other.urlList)
                    && this.isInTabGroup == other.isInTabGroup
                    && this.isIncognito == other.isIncognito
                    && Objects.equals(this.tabGroupColor, other.tabGroupColor);
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
