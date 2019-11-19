// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list;

import android.content.Context;
import android.content.res.Resources;
import android.text.format.DateUtils;
import android.text.format.Formatter;

import androidx.annotation.DrawableRes;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.download.DownloadUtils;
import org.chromium.chrome.browser.download.home.filter.Filters;
import org.chromium.chrome.browser.download.home.list.view.CircularProgressView;
import org.chromium.chrome.browser.download.home.list.view.CircularProgressView.UiState;
import org.chromium.chrome.browser.util.MathUtils;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.components.offline_items_collection.OfflineItem.Progress;
import org.chromium.components.offline_items_collection.OfflineItemFilter;
import org.chromium.components.offline_items_collection.OfflineItemProgressUnit;
import org.chromium.components.offline_items_collection.OfflineItemState;
import org.chromium.components.url_formatter.UrlFormatter;

import java.util.Calendar;
import java.util.Date;

/** A set of helper utility methods for the UI. */
public final class UiUtils {
    private static boolean sDisableUrlFormatting;

    /**
     * Disable url formatting for tests since tests might not native initialized.
     */
    @VisibleForTesting
    public static void setDisableUrlFormattingForTests(boolean disabled) {
        sDisableUrlFormatting = disabled;
    }

    private UiUtils() {}

    /**
     * Builds the accessibility text to be used for a given chip on the chips row.
     * @param resources The resources to use for lookup.
     * @param filter The filter type of the chip.
     * @param itemCount The number of items being shown on the given chip.
     * @return The content description to be used for the chip.
     */
    public static String getChipContentDescription(
            Resources resources, @Filters.FilterType int filter, int itemCount) {
        switch (filter) {
            case Filters.FilterType.NONE:
                return resources.getQuantityString(
                        R.plurals.accessibility_download_manager_ui_generic, itemCount, itemCount);
            case Filters.FilterType.VIDEOS:
                return resources.getQuantityString(
                        R.plurals.accessibility_download_manager_ui_video, itemCount, itemCount);
            case Filters.FilterType.MUSIC:
                return resources.getQuantityString(
                        R.plurals.accessibility_download_manager_ui_audio, itemCount, itemCount);
            case Filters.FilterType.IMAGES:
                return resources.getQuantityString(
                        R.plurals.accessibility_download_manager_ui_images, itemCount, itemCount);
            case Filters.FilterType.SITES:
                return resources.getQuantityString(
                        R.plurals.accessibility_download_manager_ui_pages, itemCount, itemCount);
            case Filters.FilterType.OTHER:
                return resources.getQuantityString(
                        R.plurals.accessibility_download_manager_ui_generic, itemCount, itemCount);
            default:
                assert false;
                return null;
        }
    }

    /**
     * Converts {@code date} into a string meant to be used as a list header.
     * @param date The {@link Date} to convert.
     * @return     The {@link CharSequence} representing the header.
     */
    public static CharSequence dateToHeaderString(Date date) {
        Context context = ContextUtils.getApplicationContext();

        Calendar calendar1 = CalendarFactory.get();
        Calendar calendar2 = CalendarFactory.get();

        calendar1.setTimeInMillis(System.currentTimeMillis());
        calendar2.setTime(date);

        StringBuilder builder = new StringBuilder();
        if (CalendarUtils.isSameDay(calendar1, calendar2)) {
            builder.append(context.getString(R.string.today)).append(" - ");
        } else {
            calendar1.add(Calendar.DATE, -1);
            if (CalendarUtils.isSameDay(calendar1, calendar2)) {
                builder.append(context.getString(R.string.yesterday)).append(" - ");
            }
        }

        builder.append(DateUtils.formatDateTime(context, date.getTime(),
                DateUtils.FORMAT_ABBREV_WEEKDAY | DateUtils.FORMAT_ABBREV_MONTH
                        | DateUtils.FORMAT_SHOW_YEAR));

        return builder;
    }

    /**
     * Converts {@code date} to a string meant to be used as a prefetched item timestamp.
     * @param date The {@link Date} to convert.
     * @return     The {@link CharSequence} representing the timestamp.
     */
    public static CharSequence generatePrefetchTimestamp(Date date) {
        Context context = ContextUtils.getApplicationContext();

        Calendar calendar1 = CalendarFactory.get();
        Calendar calendar2 = CalendarFactory.get();

        calendar1.setTimeInMillis(System.currentTimeMillis());
        calendar2.setTime(date);

        if (CalendarUtils.isSameDay(calendar1, calendar2)) {
            int hours = (int) MathUtils.clamp(
                    (calendar1.getTimeInMillis() - calendar2.getTimeInMillis())
                            / DateUtils.HOUR_IN_MILLIS,
                    1, 23);
            return context.getResources().getQuantityString(
                    R.plurals.download_manager_n_hours, hours, hours);
        } else {
            return DateUtils.formatDateTime(context, date.getTime(), DateUtils.FORMAT_SHOW_YEAR);
        }
    }

    /**
     * Generates a caption for a prefetched item.
     * @param item The {@link OfflineItem} to generate a caption for.
     * @return     The {@link CharSequence} representing the caption.
     */
    public static CharSequence generatePrefetchCaption(OfflineItem item) {
        Context context = ContextUtils.getApplicationContext();
        String displaySize = Formatter.formatFileSize(context, item.totalSizeBytes);
        String displayUrl = item.pageUrl;
        if (!sDisableUrlFormatting) {
            displayUrl = UrlFormatter.formatUrlForSecurityDisplayOmitScheme(item.pageUrl);
        }
        return context.getString(
                R.string.download_manager_prefetch_caption, displayUrl, displaySize);
    }

    /**
     * Generates a caption for a generic item.
     * @param item The {@link OfflineItem} to generate a caption for.
     * @return     The {@link CharSequence} representing the caption.
     */
    public static CharSequence generateGenericCaption(OfflineItem item) {
        Context context = ContextUtils.getApplicationContext();
        String displayUrl = item.pageUrl;
        if (!sDisableUrlFormatting) {
            displayUrl = UrlFormatter.formatUrlForSecurityDisplayOmitScheme(item.pageUrl);
        }

        if (item.totalSizeBytes == 0) {
            return context.getString(
                    R.string.download_manager_list_item_description_no_size, displayUrl);
        }

        String displaySize = Formatter.formatFileSize(context, item.totalSizeBytes);
        return context.getString(
                R.string.download_manager_list_item_description, displaySize, displayUrl);
    }

    /** @return Whether or not {@code item} can show a thumbnail in the UI. */
    public static boolean canHaveThumbnails(OfflineItem item) {
        switch (item.filter) {
            case OfflineItemFilter.PAGE:
            case OfflineItemFilter.VIDEO:
            case OfflineItemFilter.IMAGE:
            case OfflineItemFilter.AUDIO:
                return true;
            default:
                return false;
        }
    }

    /** @return A drawable resource id representing an icon for {@code item}. */
    public static @DrawableRes int getIconForItem(OfflineItem item) {
        switch (Filters.fromOfflineItem(item)) {
            case Filters.FilterType.NONE:
                return R.drawable.ic_file_download_24dp;
            case Filters.FilterType.SITES:
                return R.drawable.ic_globe_24dp;
            case Filters.FilterType.VIDEOS:
                return R.drawable.ic_videocam_24dp;
            case Filters.FilterType.MUSIC:
                return R.drawable.ic_music_note_24dp;
            case Filters.FilterType.IMAGES:
                return R.drawable.ic_drive_image_24dp;
            case Filters.FilterType.DOCUMENT:
                return R.drawable.ic_drive_document_24dp;
            case Filters.FilterType.OTHER: // Intentional fallthrough.
            default:
                return R.drawable.ic_drive_file_24dp;
        }
    }

    /**
     * Generates a caption for downloads that are in-progress.
     * @param item       The {@link OfflineItem} to generate a caption for.
     * @param abbreviate Whether or not to abbreviate the caption for smaller UI surfaces.
     * @return           The {@link CharSequence} representing the caption.
     */
    public static CharSequence generateInProgressCaption(OfflineItem item, boolean abbreviate) {
        return abbreviate ? generateInProgressShortCaption(item)
                          : generateInProgressLongCaption(item);
    }

    /**
     * Populates a {@link CircularProgressView} based on the contents of an {@link OfflineItem}.
     * This is a helper glue method meant to consolidate the setting of {@link CircularProgressView}
     * state.
     * @param view The {@link CircularProgressView} to update.
     * @param item The {@link OfflineItem} to use as the source of the update state.
     */
    public static void setProgressForOfflineItem(CircularProgressView view, OfflineItem item) {
        Progress progress = item.progress;
        final boolean indeterminate = progress != null && progress.isIndeterminate();
        final int determinateProgress =
                progress != null && !indeterminate ? progress.getPercentage() : 0;
        final int activeProgress =
                indeterminate ? CircularProgressView.INDETERMINATE : determinateProgress;
        final int inactiveProgress = indeterminate ? 0 : determinateProgress;

        @UiState
        int shownState;
        int shownProgress;

        switch (item.state) {
            case OfflineItemState.PENDING: // Intentional fallthrough.
            case OfflineItemState.IN_PROGRESS:
                shownState = CircularProgressView.UiState.RUNNING;
                break;
            case OfflineItemState.FAILED: // Intentional fallthrough.
            case OfflineItemState.CANCELLED:
                shownState = CircularProgressView.UiState.RETRY;
                break;
            case OfflineItemState.PAUSED:
                shownState = CircularProgressView.UiState.PAUSED;
                break;
            case OfflineItemState.INTERRUPTED:
                shownState = item.isResumable ? CircularProgressView.UiState.RUNNING
                                              : CircularProgressView.UiState.RETRY;
                break;
            case OfflineItemState.COMPLETE: // Intentional fallthrough.
            default:
                assert false : "Unexpected state for progress bar.";
                shownState = CircularProgressView.UiState.RETRY;
                break;
        }

        switch (item.state) {
            case OfflineItemState.PAUSED: // Intentional fallthrough.
            case OfflineItemState.PENDING:
                shownProgress = inactiveProgress;
                break;
            case OfflineItemState.IN_PROGRESS:
                shownProgress = activeProgress;
                break;
            case OfflineItemState.FAILED: // Intentional fallthrough.
            case OfflineItemState.CANCELLED:
                shownProgress = 0;
                break;
            case OfflineItemState.INTERRUPTED:
                shownProgress = item.isResumable ? inactiveProgress : 0;
                break;
            case OfflineItemState.COMPLETE: // Intentional fallthrough.
            default:
                assert false : "Unexpected state for progress bar.";
                shownProgress = 0;
                break;
        }

        // TODO(dtrainor): This will need to be updated once we nail down failure cases
        // (specifically non-retriable failures).
        view.setState(shownState);
        view.setProgress(shownProgress);
    }

    /**
     * Generates a detailed caption for downloads that are in-progress.
     * @param item The {@link OfflineItem} to generate a caption for.
     * @return     The {@link CharSequence} representing the caption.
     */
    private static CharSequence generateInProgressLongCaption(OfflineItem item) {
        Context context = ContextUtils.getApplicationContext();
        assert item.state != OfflineItemState.COMPLETE;

        OfflineItem.Progress progress = item.progress;

        // Make sure we have a valid OfflineItem.Progress to parse even if it's just for the failed
        // message.
        if (progress == null) {
            if (item.totalSizeBytes > 0) {
                progress = new OfflineItem.Progress(
                        0, item.totalSizeBytes, OfflineItemProgressUnit.BYTES);
            } else {
                progress = new OfflineItem.Progress(0, 100L, OfflineItemProgressUnit.PERCENTAGE);
            }
        }

        CharSequence progressString = DownloadUtils.getProgressTextForNotification(progress);
        CharSequence statusString = null;

        switch (item.state) {
            case OfflineItemState.PENDING:
                // TODO(crbug.com/891421): Add detailed pending state string from
                // DownloadUtils.getPendingStatusString().
                statusString = context.getString(R.string.download_manager_pending);
                break;
            case OfflineItemState.IN_PROGRESS:
                if (item.timeRemainingMs > 0) {
                    statusString = DownloadUtils.formatRemainingTime(context, item.timeRemainingMs);
                }
                break;
            case OfflineItemState.FAILED: // Intentional fallthrough.
            case OfflineItemState.CANCELLED: // Intentional fallthrough.
            case OfflineItemState.INTERRUPTED:
                // TODO(crbug.com/891421): Add detailed failure state string from
                // DownloadUtils.getFailStatusString().
                statusString = context.getString(R.string.download_manager_failed);
                break;
            case OfflineItemState.PAUSED:
                statusString = context.getString(R.string.download_manager_paused);
                break;
            case OfflineItemState.COMPLETE: // Intentional fallthrough.
            default:
                assert false;
        }

        if (statusString == null) return progressString;

        return context.getString(
                R.string.download_manager_in_progress_description, progressString, statusString);
    }

    /**
     * Generates a short caption for downloads that are in-progress.
     * @param item The {@link OfflineItem} to generate a short caption for.
     * @return     The {@link CharSequence} representing the caption.
     */
    private static CharSequence generateInProgressShortCaption(OfflineItem item) {
        Context context = ContextUtils.getApplicationContext();

        switch (item.state) {
            case OfflineItemState.PENDING:
                return context.getString(R.string.download_manager_pending);
            case OfflineItemState.IN_PROGRESS:
                if (item.timeRemainingMs > 0) {
                    return DownloadUtils.formatRemainingTime(context, item.timeRemainingMs);
                } else {
                    return DownloadUtils.getProgressTextForNotification(item.progress);
                }
            case OfflineItemState.FAILED: // Intentional fallthrough.
            case OfflineItemState.CANCELLED: // Intentional fallthrough.
            case OfflineItemState.INTERRUPTED:
                return context.getString(R.string.download_manager_failed);
            case OfflineItemState.PAUSED:
                return context.getString(R.string.download_manager_paused);
            case OfflineItemState.COMPLETE: // Intentional fallthrough.
            default:
                assert false;
                return "";
        }
    }
}
