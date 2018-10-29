// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list;

import android.view.View;

import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.browser.download.home.StableIds;
import org.chromium.components.offline_items_collection.OfflineItem;

import java.util.Calendar;
import java.util.Date;
import java.util.List;

/** An abstract class that represents a variety of possible list items to show in downloads home. */
public abstract class ListItem {
    private static final long DATE_SEPARATOR_HASH_CODE_OFFSET = 10;
    private static final long SECTION_SEPARATOR_HASH_CODE_OFFSET = 100;
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

        /**
         * Creates a {@link DateListItem} instance around a particular calendar day.  This will
         * automatically generate the {@link ListItem#stableId} from {@code calendar}.
         * @param calendar
         */
        public DateListItem(Calendar calendar) {
            this(generateStableIdForDayOfYear(calendar), calendar.getTime());
        }

        @VisibleForTesting
        static long generateStableIdForDayOfYear(Calendar calendar) {
            return (calendar.get(Calendar.YEAR) << 16) + calendar.get(Calendar.DAY_OF_YEAR);
        }
    }

    /** A {@link ListItem} representing a section header. */
    public static class SectionHeaderListItem extends DateListItem {
        public final int filter;
        public boolean showDate;
        public boolean showTitle;
        public boolean showMenu;
        public boolean isJustNow;
        public boolean showDivider;
        public List<OfflineItem> items;

        /**
         * Creates a {@link SectionHeaderListItem} instance for a given {@code filter} and
         * {@code timestamp}.
         */
        public SectionHeaderListItem(int filter, long timestamp, boolean showDate,
                boolean isJustNow, boolean showDivider) {
            super(isJustNow && showDate ? StableIds.JUST_NOW_SECTION
                                        : generateStableId(timestamp, filter),
                    new Date(timestamp));
            this.filter = filter;
            this.showDate = showDate;
            this.isJustNow = isJustNow;
            this.showDivider = showDivider;
        }

        @VisibleForTesting
        static long generateStableId(long timestamp, int filter) {
            long hash = new Date(timestamp).hashCode();
            return hash + filter + SECTION_HEADER_HASH_CODE_OFFSET;
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
