// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.list;

import android.content.Context;
import android.content.res.Resources;
import android.text.format.DateUtils;
import android.text.format.Formatter;

import androidx.annotation.DrawableRes;

import org.chromium.base.BuildInfo;
import org.chromium.base.ContextUtils;
import org.chromium.base.MathUtils;
import org.chromium.chrome.browser.download.StringUtils;
import org.chromium.chrome.browser.download.home.filter.Filters;
import org.chromium.chrome.browser.download.home.list.view.CircularProgressView;
import org.chromium.chrome.browser.download.home.list.view.CircularProgressView.UiState;
import org.chromium.chrome.browser.download.internal.R;
import org.chromium.components.browser_ui.util.DownloadUtils;
import org.chromium.components.browser_ui.util.date.CalendarFactory;
import org.chromium.components.browser_ui.util.date.CalendarUtils;
import org.chromium.components.offline_items_collection.LegacyHelpers;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.components.offline_items_collection.OfflineItem.Progress;
import org.chromium.components.offline_items_collection.OfflineItemFilter;
import org.chromium.components.offline_items_collection.OfflineItemProgressUnit;
import org.chromium.components.offline_items_collection.OfflineItemState;
import org.chromium.components.url_formatter.SchemeDisplay;
import org.chromium.components.url_formatter.UrlFormatter;

import java.util.Calendar;
import java.util.Date;

/** A set of helper utility methods for the UI. */
public final class UiUtils {
    // Limit file name to 25 characters.
    public static final int MAX_FILE_NAME_LENGTH_FOR_TITLE = 33;

    private UiUtils() {}

    static {
        CalendarFactory.warmUp();
    }

    /**
     * Builds the accessibility text to be used for a given chip on the chips row.
     *
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
                        R.plurals.accessibility_download_manager_ui_all, itemCount, itemCount);
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
                        R.plurals.accessibility_download_manager_ui_other, itemCount, itemCount);
            default:
                assert false;
                return null;
        }
    }

    /**
     * Converts {@code date} to a string meant to be used as a prefetched item timestamp.
     *
     * @param date The {@link Date} to convert.
     * @return The {@link CharSequence} representing the timestamp.
     */
    public static CharSequence generatePrefetchTimestamp(Date date) {
        Context context = ContextUtils.getApplicationContext();

        Calendar calendar1 = CalendarFactory.get();
        Calendar calendar2 = CalendarFactory.get();

        calendar1.setTimeInMillis(System.currentTimeMillis());
        calendar2.setTime(date);

        if (CalendarUtils.isSameDay(calendar1, calendar2)) {
            int hours =
                    (int)
                            MathUtils.clamp(
                                    (calendar1.getTimeInMillis() - calendar2.getTimeInMillis())
                                            / DateUtils.HOUR_IN_MILLIS,
                                    1,
                                    23);
            return context.getResources()
                    .getQuantityString(R.plurals.download_manager_n_hours, hours, hours);
        } else {
            return DateUtils.formatDateTime(context, date.getTime(), DateUtils.FORMAT_SHOW_YEAR);
        }
    }

    /**
     * Generates a caption for a prefetched item.
     *
     * @param item The {@link OfflineItem} to generate a caption for.
     * @return The {@link CharSequence} representing the caption.
     */
    public static CharSequence generatePrefetchCaption(OfflineItem item) {
        Context context = ContextUtils.getApplicationContext();
        String displaySize = Formatter.formatFileSize(context, item.totalSizeBytes);
        String displayUrl =
                UrlFormatter.formatUrlForSecurityDisplay(
                        item.url, SchemeDisplay.OMIT_HTTP_AND_HTTPS);
        return context.getString(
                R.string.download_manager_prefetch_caption, displayUrl, displaySize);
    }

    /**
     * Generates a caption for a generic item.
     *
     * @param item The {@link OfflineItem} to generate a caption for.
     * @return The {@link CharSequence} representing the caption.
     */
    public static CharSequence generateGenericCaption(OfflineItem item) {
        Context context = ContextUtils.getApplicationContext();
        String displayUrl =
                DownloadUtils.formatUrlForDisplayInNotification(
                        item.url, DownloadUtils.MAX_ORIGIN_LENGTH_FOR_DOWNLOAD_HOME_CAPTION);

        if (item.totalSizeBytes == 0) {
            return context.getString(
                    R.string.download_manager_list_item_description_no_size, displayUrl);
        }

        String displaySize = Formatter.formatFileSize(context, item.totalSizeBytes);
        return context.getString(
                R.string.download_manager_list_item_description, displaySize, displayUrl);
    }

    /**
     * Formats the file name of the download with respect to the available file size.
     *
     * @param item The {@link OfflineItem} to generate a title for.
     * @return The {@link CharSequence} representing the title.
     */
    public static CharSequence formatGenericItemTitle(OfflineItem item) {
        return StringUtils.getAbbreviatedFileName(item.title, MAX_FILE_NAME_LENGTH_FOR_TITLE);
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
     * @return A drawable resource id representing the small media icon to be shown on prefetch
     *         cards.
     */
    public static @DrawableRes int getMediaPlayIconForPrefetchCards(OfflineItem item) {
        switch (item.filter) {
            case OfflineItemFilter.VIDEO: // fallthrough
            case OfflineItemFilter.AUDIO:
                // TODO(shaktisahu): Provide vector icon for audio.
                return R.drawable.ic_play_circle_filled_24dp;
            default:
                return 0;
        }
    }

    /**
     * Generates a caption for downloads that are in-progress.
     * @param item       The {@link OfflineItem} to generate a caption for.
     * @param abbreviate Whether or not to abbreviate the caption for smaller UI surfaces.
     * @return           The {@link CharSequence} representing the caption.
     */
    public static CharSequence generateInProgressCaption(OfflineItem item, boolean abbreviate) {
        return abbreviate
                ? generateInProgressShortCaption(item)
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

        @UiState int shownState;
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
                shownState =
                        item.isResumable
                                ? CircularProgressView.UiState.RUNNING
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
                progress =
                        new OfflineItem.Progress(
                                0, item.totalSizeBytes, OfflineItemProgressUnit.BYTES);
            } else {
                progress = new OfflineItem.Progress(0, 100L, OfflineItemProgressUnit.PERCENTAGE);
            }
        }

        CharSequence progressString = StringUtils.getProgressTextForUi(progress);
        CharSequence statusString = null;

        switch (item.state) {
            case OfflineItemState.PENDING:
                // TODO(crbug.com/41418523): Add detailed pending state string from
                // StringUtils.getPendingStatusForUi().
                statusString = context.getString(R.string.download_manager_pending);
                break;
            case OfflineItemState.IN_PROGRESS:
                if (item.timeRemainingMs > 0) {
                    statusString = StringUtils.timeLeftForUi(context, item.timeRemainingMs);
                }
                break;
            case OfflineItemState.FAILED: // Intentional fallthrough.
            case OfflineItemState.CANCELLED: // Intentional fallthrough.
            case OfflineItemState.INTERRUPTED:
                // TODO(crbug.com/41418523): Add detailed failure state string from
                // StringUtils.getFailStatusForUi().
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
                    return StringUtils.timeLeftForUi(context, item.timeRemainingMs);
                } else {
                    return StringUtils.getProgressTextForUi(item.progress);
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

    /** @return Whether the given {@link OfflineItem} can be shared. */
    public static boolean canShare(OfflineItem item) {
        // Sharing functionality that leads directly to the Android share sheet is
        // currently disabled.
        if (BuildInfo.getInstance().isAutomotive) {
            return false;
        }
        return (item.state == OfflineItemState.COMPLETE)
                && (LegacyHelpers.isLegacyDownload(item.id)
                        || LegacyHelpers.isLegacyOfflinePage(item.id));
    }

    /** @return The domain associated with the given {@link OfflineItem}. */
    public static String getDomainForItem(OfflineItem offlineItem) {
        String formattedUrl =
                UrlFormatter.formatUrlForSecurityDisplay(
                        offlineItem.url, SchemeDisplay.OMIT_HTTP_AND_HTTPS);
        return formattedUrl;
    }
}
