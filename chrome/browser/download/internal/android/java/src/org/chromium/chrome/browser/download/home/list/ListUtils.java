// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list;

import androidx.annotation.IntDef;

import org.chromium.chrome.browser.download.home.DownloadManagerUiConfig;
import org.chromium.chrome.browser.download.home.filter.Filters.FilterType;
import org.chromium.chrome.browser.download.home.list.ListItem.OfflineItemListItem;
import org.chromium.chrome.browser.download.home.list.ListItem.ViewListItem;
import org.chromium.components.browser_ui.util.date.CalendarUtils;
import org.chromium.components.offline_items_collection.LegacyHelpers;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.components.offline_items_collection.OfflineItemFilter;
import org.chromium.components.offline_items_collection.OfflineItemState;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Date;
import java.util.List;

/** Utility methods for representing {@link ListItem}s in a {@link RecyclerView} list. */
public class ListUtils {
    /** The potential types of list items that could be displayed. */
    @IntDef({
        ViewType.DATE,
        ViewType.IN_PROGRESS,
        ViewType.GENERIC,
        ViewType.VIDEO,
        ViewType.AUDIO,
        ViewType.IMAGE,
        ViewType.IMAGE_FULL_WIDTH,
        ViewType.CUSTOM_VIEW,
        ViewType.SECTION_HEADER,
        ViewType.IN_PROGRESS_VIDEO,
        ViewType.IN_PROGRESS_IMAGE,
        ViewType.PREFETCH_ARTICLE,
        ViewType.GROUP_CARD_ITEM,
        ViewType.GROUP_CARD_HEADER,
        ViewType.GROUP_CARD_FOOTER,
        ViewType.PAGINATION_HEADER,
        ViewType.GROUP_CARD_DIVIDER_TOP,
        ViewType.GROUP_CARD_DIVIDER_MIDDLE,
        ViewType.GROUP_CARD_DIVIDER_BOTTOM
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface ViewType {
        int DATE = 0;
        int IN_PROGRESS = 1;
        int GENERIC = 2;
        int VIDEO = 3;
        int AUDIO = 4;
        int IMAGE = 5;
        int IMAGE_FULL_WIDTH = 6;
        int CUSTOM_VIEW = 7;
        int SECTION_HEADER = 8;
        int IN_PROGRESS_VIDEO = 9;
        int IN_PROGRESS_IMAGE = 10;
        int PREFETCH_ARTICLE = 11;
        int GROUP_CARD_ITEM = 12;
        int GROUP_CARD_HEADER = 13;
        int GROUP_CARD_FOOTER = 14;
        int GROUP_CARD_DIVIDER_TOP = 15;
        int GROUP_CARD_DIVIDER_MIDDLE = 16;
        int GROUP_CARD_DIVIDER_BOTTOM = 17;
        int PAGINATION_HEADER = 18;
    }

    /**
     * A visual ordering of the {@link Filters#FilterType}s to determine what order the sections
     * should appear in the UI.
     *
     * Note that this list should have an entry for each {@link Filters#FilterType} that can be
     * shown visually and asserts will fire if it does not.
     */
    private static final int[] FILTER_TYPE_ORDER_LIST =
            new int[] {
                FilterType.NONE,
                FilterType.VIDEOS,
                FilterType.MUSIC,
                FilterType.IMAGES,
                FilterType.SITES,
                FilterType.OTHER,
                FilterType.DOCUMENT,
                FilterType.PREFETCHED
            };

    /** Converts a given list of {@link ListItem}s to a list of {@link OfflineItem}s. */
    public static List<OfflineItem> toOfflineItems(Collection<ListItem> items) {
        List<OfflineItem> offlineItems = new ArrayList<>();
        for (ListItem item : items) {
            if (item instanceof ListItem.OfflineItemListItem) {
                offlineItems.add(((ListItem.OfflineItemListItem) item).item);
            }
        }
        return offlineItems;
    }

    /**
     * Analyzes a {@link ListItem} and finds the most appropriate {@link ViewType} based on the
     * current state.
     * @param item   The {@link ListItem} to determine the {@link ViewType} for.
     * @param config The {@link DownloadManagerUiConfig}.
     * @return       The type of {@link ViewType} to use for a particular {@link ListItem}.
     * @see          ViewType
     */
    public static @ViewType int getViewTypeForItem(ListItem item, DownloadManagerUiConfig config) {
        if (item instanceof ViewListItem) return ViewType.CUSTOM_VIEW;
        if (item instanceof ListItem.SectionHeaderListItem) return ViewType.SECTION_HEADER;
        if (item instanceof ListItem.PaginationListItem) return ViewType.PAGINATION_HEADER;
        if (item instanceof ListItem.CardHeaderListItem) {
            return ViewType.GROUP_CARD_HEADER;
        }
        if (item instanceof ListItem.CardFooterListItem) {
            return ViewType.GROUP_CARD_FOOTER;
        }

        if (item instanceof ListItem.CardDividerListItem) {
            switch (((ListItem.CardDividerListItem) item).position) {
                case TOP:
                    return ViewType.GROUP_CARD_DIVIDER_TOP;
                case MIDDLE:
                    return ViewType.GROUP_CARD_DIVIDER_MIDDLE;
                case BOTTOM:
                    return ViewType.GROUP_CARD_DIVIDER_BOTTOM;
            }
        }

        if (item instanceof OfflineItemListItem) {
            OfflineItemListItem offlineItem = (OfflineItemListItem) item;
            if (offlineItem.isGrouped) return ViewType.GROUP_CARD_ITEM;

            boolean inProgress =
                    offlineItem.item.state == OfflineItemState.IN_PROGRESS
                            || offlineItem.item.state == OfflineItemState.PAUSED
                            || offlineItem.item.state == OfflineItemState.INTERRUPTED
                            || offlineItem.item.state == OfflineItemState.PENDING
                            || offlineItem.item.state == OfflineItemState.FAILED;

            if (config.useGenericViewTypes) {
                return inProgress ? ViewType.IN_PROGRESS : ViewType.GENERIC;
            }

            if (offlineItem.item.isSuggested) {
                if (offlineItem.item.filter == OfflineItemFilter.PAGE) {
                    return ViewType.PREFETCH_ARTICLE;
                } else if (offlineItem.item.filter == OfflineItemFilter.AUDIO) {
                    return ViewType.AUDIO;
                }
            }

            switch (offlineItem.item.filter) {
                case OfflineItemFilter.VIDEO:
                    return inProgress ? ViewType.IN_PROGRESS_VIDEO : ViewType.VIDEO;
                case OfflineItemFilter.IMAGE:
                    return inProgress
                            ? ViewType.IN_PROGRESS_IMAGE
                            : (offlineItem.spanFullWidth
                                    ? ViewType.IMAGE_FULL_WIDTH
                                    : ViewType.IMAGE);
                    // case OfflineItemFilter.PAGE:
                    // case OfflineItemFilter.AUDIO:
                    // case OfflineItemFilter.OTHER:
                    // case OfflineItemFilter.DOCUMENT:
                default:
                    return inProgress ? ViewType.IN_PROGRESS : ViewType.GENERIC;
            }
        }

        assert false;
        return ViewType.GENERIC;
    }

    /** @return Whether the given {@link ListItem} can be grouped inside a card. */
    public static boolean canGroup(ListItem listItem) {
        if (!(listItem instanceof OfflineItemListItem)) return false;
        return LegacyHelpers.isLegacyContentIndexedItem(((OfflineItemListItem) listItem).item.id);
    }

    /**
     * Analyzes a {@link ListItem} and finds the best span size based on the current state.  Span
     * size determines how many columns this {@link ListItem}'s {@link View} will take up in the
     * overall list.
     * @param item      The {@link ListItem} to determine the span size for.
     * @param config    The {@link DownloadManagerUiConfig}.
     * @param spanCount The maximum span amount of columns {@code item} can take up.
     * @return          The number of columns {@code item} should take.
     * @see             GridLayoutManager.SpanSizeLookup
     */
    public static int getSpanSize(ListItem item, DownloadManagerUiConfig config, int spanCount) {
        switch (getViewTypeForItem(item, config)) {
            case ViewType.IMAGE: // Intentional fallthrough.
            case ViewType.IN_PROGRESS_IMAGE:
                return 1;
            default:
                return spanCount;
        }
    }

    /**
     * Helper method to determine which item type section to show first in the list.
     * @return -1 if {@code a} should be shown before {@code b}.
     *          0 if {@code a} == {@code b}.
     *          1 if {@code a} should be shown after {@code b}.
     */
    public static int compareFilterTypesTo(@FilterType int a, @FilterType int b) {
        int aPriority = getVisualPriorityForFilter(a);
        int bPriority = getVisualPriorityForFilter(b);
        return (aPriority < bPriority) ? -1 : ((aPriority == bPriority) ? 0 : 1);
    }

    /**
     * Helper method to compare list items based on date. Two items have equal if they both got
     * created on the same day.
     * @return -1 if {@code a} should be shown before {@code b}.
     *          0 if {@code a} == {@code b}.
     *          1 if {@code a} should be shown after {@code b}.
     */
    public static int compareItemByDate(OfflineItem a, OfflineItem b) {
        Date aDay = CalendarUtils.getStartOfDay(a.creationTimeMs).getTime();
        Date bDay = CalendarUtils.getStartOfDay(b.creationTimeMs).getTime();
        return bDay.compareTo(aDay);
    }

    /**
     * Helper method to compare list items based on timestamp.
     * @return -1 if {@code a} should be shown before {@code b}.
     *          0 if {@code a} == {@code b}.
     *          1 if {@code a} should be shown after {@code b}.
     */
    public static int compareItemByTimestamp(OfflineItem a, OfflineItem b) {
        return Long.compare(b.creationTimeMs, a.creationTimeMs);
    }

    /**
     * Helper method to compare list items based on ID.
     * @return -1 if {@code a} should be shown before {@code b}.
     *          0 if {@code a} == {@code b}.
     *          1 if {@code a} should be shown after {@code b}.
     */
    public static int compareItemByID(OfflineItem a, OfflineItem b) {
        int comparison = a.id.namespace.compareTo(b.id.namespace);
        if (comparison != 0) return comparison;
        return a.id.id.compareTo(b.id.id);
    }

    private static int getVisualPriorityForFilter(@FilterType int type) {
        for (int i = 0; i < FILTER_TYPE_ORDER_LIST.length; i++) {
            if (FILTER_TYPE_ORDER_LIST[i] == type) return i;
        }

        assert false
                : "Unexpected Filters.FilterType (did you forget to update FILTER_TYPE_ORDER_LIST?).";
        return 0;
    }
}
