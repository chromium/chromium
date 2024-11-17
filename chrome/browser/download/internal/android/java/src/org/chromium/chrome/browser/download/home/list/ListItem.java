// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list;

import android.util.Pair;
import android.view.View;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.download.home.StableIds;
import org.chromium.components.offline_items_collection.OfflineItem;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
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
    public abstract static class DateListItem extends ListItem {
        public final Date date;

        /**
         * Creates a {@link DateListItem} instance. with a predefined {@code stableId} and {@code
         * date}.
         */
        public DateListItem(long stableId, Date date) {
            super(stableId);
            this.date = date;
        }
    }

    /** A {@link ListItem} representing group card decoration such as header or footer. */
    public static class CardDecorationListItem extends ListItem {
        public final Pair<Date, String> dateAndDomain;

        /** Creates a {@link CardDecorationListItem} instance. */
        public CardDecorationListItem(Pair<Date, String> dateAndDomain, boolean isHeader) {
            super(generateStableId(dateAndDomain, isHeader));
            this.dateAndDomain = dateAndDomain;
        }

        @VisibleForTesting
        static long generateStableId(Pair<Date, String> dateAndDomain, boolean isHeader) {
            return isHeader ? dateAndDomain.hashCode() : ~dateAndDomain.hashCode();
        }
    }

    /** A {@link ListItem} representing a card header. */
    public static class CardHeaderListItem extends CardDecorationListItem {
        public String faviconUrl;

        /** Creates a {@link CardHeaderListItem} instance. */
        public CardHeaderListItem(Pair<Date, String> dateAndDomain, String faviconUrl) {
            super(dateAndDomain, true);
            this.faviconUrl = faviconUrl;
        }
    }

    /** A {@link ListItem} representing a card footer. */
    public static class CardFooterListItem extends CardDecorationListItem {
        /** Creates a {@link CardFooterListItem} instance. */
        public CardFooterListItem(Pair<Date, String> dateAndDomain) {
            super(dateAndDomain, false);
        }
    }

    /** A {@link ListItem} representing a divider in a group card. */
    public static class CardDividerListItem extends ListItem {
        /** The position of the divider in a group card. */
        public enum Position {
            /** Represents the curved border at the top of a group card. */
            TOP,

            /**
             * Represents the line divider between two items in a group card. It also contains
             * two side bars on left and right to make up for the padding between two items.
             */
            MIDDLE,

            /** Represents the curved border at the bottom of a group card. */
            BOTTOM
        }

        public final Position position;

        /** Creates a {@link CardDividerListItem} instance for a given position. */
        public CardDividerListItem(long stableId, Position position) {
            super(stableId);
            this.position = position;
        }
    }

    /** The type of the section header. */
    @IntDef({SectionHeaderType.INVALID, SectionHeaderType.DATE, SectionHeaderType.JUST_NOW})
    @Retention(RetentionPolicy.SOURCE)
    public @interface SectionHeaderType {
        int INVALID = -1;
        int DATE = 0;
        int JUST_NOW = 1;
    }

    /** A {@link ListItem} representing a section header. */
    public static class SectionHeaderListItem extends DateListItem {
        public @SectionHeaderType int type;

        /** Creates a {@link SectionHeaderListItem} instance for a given {@code timestamp}. */
        public SectionHeaderListItem(long timestamp, @SectionHeaderType int type) {
            super(generateStableId(type, timestamp), new Date(timestamp));
            this.type = type;
        }

        @VisibleForTesting
        static long generateStableId(@SectionHeaderType int type, long timestamp) {
            switch (type) {
                case SectionHeaderType.DATE:
                    long hash = new Date(timestamp).hashCode();
                    return hash + SECTION_HEADER_HASH_CODE_OFFSET;
                case SectionHeaderType.JUST_NOW:
                    return StableIds.JUST_NOW_SECTION;
            }
            assert false : "Unknown section header type.";
            return -1;
        }
    }

    /** A {@link ListItem} that involves a {@link OfflineItem}. */
    public static class OfflineItemListItem extends DateListItem {
        public OfflineItem item;
        public boolean spanFullWidth;
        public boolean isGrouped;

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
