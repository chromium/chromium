// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list;

import android.view.View;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.download.home.StableIds;
import org.chromium.components.offline_items_collection.OfflineItem;

import java.util.Date;

/** An abstract class that represents a variety of possible list items to show in downloads home. */
public abstract class ListItem {
    private static final long SECTION_HEADER_HASH_CODE_OFFSET = 1000;

    public final long stableId;

    /** Indicates that we are in multi-select mode and the item is currently selected. */
    public boolean selected;

    /** Whether animation should be shown for the recent change in selection state for this item. */
    public boolean showSelectedAnimation;

    /** Creates a {@link ListItem} instance. */
    ListItem(long stableId) {
        this.stableId = stableId;
    }

    /** A {@link ListItem} that exposes a custom {@link View} to show. */
    public static class ViewListItem extends ListItem {
        public final View customView;

        /** Creates a {@link ViewListItem} instance. */
        public ViewListItem(long stableId, View customView) {
            super(stableId);
            this.customView = customView;
        }
    }

    /** A {@link ListItem} representing a pagination header. */
    public static class PaginationListItem extends ListItem {
        /** Creates a {@link PaginationListItem} instance. */
        public PaginationListItem() {
            super(StableIds.PAGINATION_HEADER);
        }
    }

    /** A {@link ListItem} that involves a {@link Date}. */
    private abstract static class DateListItem extends ListItem {
        public final Date date;

        /**
         * Creates a {@link DateListItem} instance. with a predefined {@code stableId} and
         * {@code date}.
         */
        public DateListItem(long stableId, Date date) {
            super(stableId);
            this.date = date;
        }
    }

    /** A {@link ListItem} representing a section header. */
    public static class SectionHeaderListItem extends DateListItem {
        public boolean isJustNow;
        public boolean showDivider;

        /**
         * Creates a {@link SectionHeaderListItem} instance for a given {@code timestamp}.
         */
        public SectionHeaderListItem(long timestamp, boolean isJustNow, boolean showDivider) {
            super(isJustNow ? StableIds.JUST_NOW_SECTION : generateStableId(timestamp),
                    new Date(timestamp));
            this.isJustNow = isJustNow;
            this.showDivider = showDivider;
        }

        @VisibleForTesting
        static long generateStableId(long timestamp) {
            long hash = new Date(timestamp).hashCode();
            return hash + SECTION_HEADER_HASH_CODE_OFFSET;
        }
    }

    /** A {@link ListItem} that involves a {@link OfflineItem}. */
    public static class OfflineItemListItem extends DateListItem {
        public OfflineItem item;
        public boolean spanFullWidth;

        /** Creates an {@link OfflineItemListItem} wrapping {@code item}. */
        public OfflineItemListItem(OfflineItem item) {
            super(generateStableId(item), new Date(item.creationTimeMs));
            this.item = item;
        }

        @VisibleForTesting
        static long generateStableId(OfflineItem item) {
            return (((long) item.id.hashCode()) << 32) + (item.creationTimeMs & 0x0FFFFFFFF);
        }
    }
}
