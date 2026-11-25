// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.tray;

import static org.chromium.build.NullUtil.assertNonNull;
import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.text.format.DateUtils;
import android.util.ArrayMap;
import android.util.ArraySet;
import android.view.View;

import androidx.annotation.DrawableRes;
import androidx.annotation.VisibleForTesting;
import androidx.core.text.BidiFormatter;
import androidx.core.text.TextDirectionHeuristicsCompat;

import org.chromium.base.MathUtils;
import org.chromium.base.TimeUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.download.DownloadIconUtils;
import org.chromium.chrome.browser.download.DownloadStateModel;
import org.chromium.chrome.browser.download.R;
import org.chromium.chrome.browser.download.StringUtils;
import org.chromium.chrome.browser.download.tray.DownloadTrayItemProperties.ActionButton;
import org.chromium.components.browser_ui.util.DownloadUtils;
import org.chromium.components.offline_items_collection.ContentId;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.components.offline_items_collection.OfflineItemProgressUnit;
import org.chromium.components.offline_items_collection.OfflineItemState;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.TimeUnit;
import java.util.function.BooleanSupplier;
import java.util.function.Predicate;
import java.util.function.Supplier;

/**
 * Decides when the download tray shows downloads and builds the models for its rows. Mirrors the
 * desktop download bubble's partial view: once nothing is downloading, every download this window
 * saw complete that the user hasn't acted on yet is listed in a popup anchored to the toolbar
 * button. The button opens the full tray of downloads in progress and recent ones, as desktop's
 * main view.
 */
@NullMarked
class DownloadTrayMediator implements DownloadStateModel.Observer {
    /** Shows and dismisses the tray popup on the mediator's behalf. */
    interface PopupController {
        /**
         * Shows {@code rows} in a popup anchored to {@code anchor}, replacing any shown popup. The
         * list is live: rows changed or removed while the popup shows are reflected in it.
         *
         * @return Whether the popup is showing, e.g. false if there is no room for it.
         */
        boolean showCompletions(View anchor, ModelList rows);

        /** As {@link #showCompletions}, but shows {@code rows} as the full tray. */
        boolean showFullTray(View anchor, ModelList rows);

        /** Dismisses the popup, if showing. */
        void dismiss();
    }

    /**
     * Minimum time between two completion popups, so a burst of completions doesn't keep popping
     * up. Mirrors desktop's {@code kShowPartialViewMinInterval}.
     */
    @VisibleForTesting static final long MIN_SHOW_INTERVAL_MS = TimeUnit.SECONDS.toMillis(15);

    private final Context mContext;
    private final DownloadStateModel mModel;
    private final Supplier<@Nullable View> mAnchorSupplier;
    private final BooleanSupplier mIsWindowFocused;
    private final BooleanSupplier mIsIncognitoSelected;
    private final Runnable mOnActioned;
    private final DownloadTrayDelegate mDelegate;
    private final PopupController mPopupController;
    private final ActionButton mPauseAction;
    private final ActionButton mResumeAction;
    private final ActionButton mRetryAction;
    private final ActionButton mCancelAction;

    /**
     * Downloads this window saw complete that the user hasn't acted on, as desktop's {@code
     * !WasActionedOn()}. Their data comes from {@link DownloadStateModel#getRecentEndedItems} when
     * shown, so ones that have aged out of it count as actioned, as on desktop. These sets and maps
     * hold at most a few dozen entries (bounded by that list), so array-backed ones suffice.
     */
    private final Set<ContentId> mUnactionedIds = new ArraySet<>();

    /** The rows in the showing popup by download; empty if no popup is showing. */
    private final Map<ContentId, ListItem> mShownRows = new ArrayMap<>();

    /** The showing popup's list, or null if no popup is showing. */
    private @Nullable ModelList mShownList;

    /** Whether the showing popup is the full tray rather than the completion popup. */
    private boolean mIsFullTrayShowing;

    /**
     * Completions since the last popup, waiting to be shown once nothing is still downloading. Kept
     * so they are still shown if the last active download fails, is cancelled or is removed.
     */
    private final Set<ContentId> mPendingIds = new ArraySet<>();

    /**
     * When the last completion popup was shown. One interval in the past, as it starts, lets the
     * next one show right away.
     */
    private long mLastShownTimeMs = -MIN_SHOW_INTERVAL_MS;

    /**
     * @param context The context used to resolve resources.
     * @param model The model to observe for download state changes.
     * @param anchorSupplier Supplies the view to anchor popups to, or null if unavailable.
     * @param isWindowFocused Whether this is the window the user last focused; only that window
     *     shows popups.
     * @param isIncognitoSelected Whether this window is showing incognito tabs.
     * @param onActioned Invoked when the user acts on a download from the tray.
     * @param delegate Performs actions that need dependencies outside the download module.
     * @param popupController Shows and dismisses the popup.
     */
    DownloadTrayMediator(
            Context context,
            DownloadStateModel model,
            Supplier<@Nullable View> anchorSupplier,
            BooleanSupplier isWindowFocused,
            BooleanSupplier isIncognitoSelected,
            Runnable onActioned,
            DownloadTrayDelegate delegate,
            PopupController popupController) {
        mContext = context;
        mModel = model;
        mAnchorSupplier = anchorSupplier;
        mIsWindowFocused = isWindowFocused;
        mIsIncognitoSelected = isIncognitoSelected;
        mOnActioned = onActioned;
        mDelegate = delegate;
        mPopupController = popupController;
        mPauseAction =
                new ActionButton(
                        R.drawable.ic_pause_white_24dp,
                        R.string.download_notification_pause_button,
                        R.string.download_tray_item_pause_accessibility,
                        mModel::pauseDownload);
        mResumeAction =
                new ActionButton(
                        R.drawable.ic_play_arrow_white_24dp,
                        R.string.download_notification_resume_button,
                        R.string.download_tray_item_resume_accessibility,
                        mModel::resumeDownload);
        mRetryAction =
                new ActionButton(
                        R.drawable.ic_refresh_24dp,
                        R.string.download_notification_resume_button,
                        R.string.download_tray_item_retry_accessibility,
                        mModel::resumeDownload);
        mCancelAction =
                new ActionButton(
                        R.drawable.material_ic_close_24dp,
                        R.string.download_notification_cancel_button,
                        R.string.download_tray_item_cancel_accessibility,
                        mModel::cancelDownload);
        mModel.addObserver(this);
    }

    /** Stops observing downloads and dismisses any popup this mediator showed. */
    void destroy() {
        mModel.removeObserver(this);
        mPopupController.dismiss();
    }

    /** Called by the popup controller whenever the popup is dismissed, for any reason. */
    void onDismissed() {
        mShownRows.clear();
        mShownList = null;
        mIsFullTrayShowing = false;
    }

    /**
     * Opens the full tray of this window's recent downloads, or closes it if it is open. As
     * desktop's main view, listed downloads count as acted on, and with nothing to list the
     * download history opens instead.
     */
    void toggleFullTray() {
        if (mIsFullTrayShowing) {
            mPopupController.dismiss();
            return;
        }
        View anchor = mAnchorSupplier.get();
        if (anchor == null || !anchor.isShown()) return;

        // As on desktop, opening the tray resets the completion popup's minimum interval.
        mLastShownTimeMs = -MIN_SHOW_INTERVAL_MS;

        Map<ContentId, ListItem> rowsById = new ArrayMap<>();
        ModelList rows = buildFullTrayRows(rowsById);
        if (rows.isEmpty()) {
            openDownloadHistory();
            return;
        }
        // Showing first dismisses any completion popup, which clears the shown rows, so record the
        // new ones only once the tray is showing.
        if (!mPopupController.showFullTray(anchor, rows)) return;
        mShownList = rows;
        mShownRows.putAll(rowsById);
        mIsFullTrayShowing = true;
        mUnactionedIds.removeAll(rowsById.keySet());
    }

    /** Called when the user asks for the full download history from the tray. */
    void onFullHistoryClicked() {
        mPopupController.dismiss();
        openDownloadHistory();
    }

    private void openDownloadHistory() {
        mDelegate.showDownloadManager(mIsIncognitoSelected.getAsBoolean());
    }

    // DownloadStateModel.Observer implementation:

    @Override
    public void onItemChanged(OfflineItem item, @Nullable OfflineItem previous) {
        ContentId id = assumeNonNull(item.id); // The model only reports items with ids.
        boolean completed = item.state == OfflineItemState.COMPLETE && !isDangerous(item);
        if (!completed) {
            // Not, or no longer, a completion, e.g. retried or flagged as dangerous.
            dropCompletion(id);
        }
        ListItem shownRow = mShownRows.get(id);
        if (shownRow != null) {
            if (shouldKeepShownRow(item)) {
                // A shown row follows its download, e.g. as it progresses, ends or is renamed.
                updateRow(shownRow.model, item, previous);
            } else {
                // E.g. cancelled or flagged as dangerous, as if removed.
                removeShownRow(id);
            }
        }

        // The model only caches active downloads, so a null previous means it wasn't active.
        if (previous == null) return;
        // As on desktop, an active download ends when it stops being active or is flagged as
        // dangerous (desktop counts those as done). Pausing or waiting to auto-resume isn't an
        // ending: a waiting completion is shown once that download ends.
        if (DownloadStateModel.isActive(item) && !isDangerous(item)) return;

        // Only a download this window saw in progress that has just completed is shown. It is
        // remembered even if the window doesn't show it right now, e.g. an incognito download
        // while regular tabs are shown, so it is listed once the window switches.
        if (completed) {
            mUnactionedIds.add(id);
        }

        // Downloads this window doesn't show neither open its popup nor hold it back.
        if (!DownloadStateModel.isVisibleInWindow(item, mIsIncognitoSelected.getAsBoolean())) {
            return;
        }
        if (completed) {
            mPendingIds.add(id);
        }
        // Any active download ending (completed or not) may leave nothing downloading.
        maybeShowPendingCompletions();
    }

    @Override
    public void onItemRemoved(ContentId id, @Nullable OfflineItem previous) {
        // E.g. deleted from the Downloads page while its popup is showing.
        dropCompletion(id);
        removeShownRow(id);
        if (previous != null) {
            // An active download removed, e.g. cancelled, may leave nothing downloading.
            maybeShowPendingCompletions();
        }
    }

    /** Drops {@code id} from the un-actioned and pending completions. */
    private void dropCompletion(ContentId id) {
        mUnactionedIds.remove(id);
        mPendingIds.remove(id);
    }

    /** Takes {@code id}'s row out of the showing popup, closing it once nothing is left. */
    private void removeShownRow(ContentId id) {
        ListItem shownRow = mShownRows.remove(id);
        if (shownRow == null || mShownList == null) return;
        mShownList.remove(shownRow);
        if (mShownRows.isEmpty()) {
            mPopupController.dismiss();
        }
    }

    /** Whether the shown popup still lists {@code item}, so its row stays. */
    private boolean shouldKeepShownRow(OfflineItem item) {
        if (isDangerous(item)) {
            return false;
        }
        if (DownloadStateModel.isActive(item)) {
            // Only the full tray lists downloads in progress; the completion popup doesn't.
            return mIsFullTrayShowing;
        }
        // Ended: completed and interrupted downloads are listed, cancelled and failed ones aren't.
        return DownloadStateModel.isListedWhenEnded(item);
    }

    private void maybeShowPendingCompletions() {
        if (mPendingIds.isEmpty() || isAnythingDownloading()) return;
        mPendingIds.clear();

        // As on desktop, a completion while the popup is up isn't added to it; it's listed the next
        // time the popup opens.
        if (mShownList != null || !mIsWindowFocused.getAsBoolean()) return;

        long now = TimeUtils.elapsedRealtimeMillis();
        if (now - mLastShownTimeMs < MIN_SHOW_INTERVAL_MS) return;

        View anchor = mAnchorSupplier.get();
        if (anchor == null || !anchor.isShown()) return;

        Map<ContentId, ListItem> rowsById = new ArrayMap<>();
        ModelList rows = buildCompletionPopupRows(rowsById);
        if (rows.isEmpty()) return;

        // Showing replaces any previous popup, whose dismissal clears the shown rows, so set them
        // after.
        if (!mPopupController.showCompletions(anchor, rows)) return;
        mLastShownTimeMs = now;
        mShownList = rows;
        mShownRows.putAll(rowsById);
    }

    /** Builds the completion popup's rows: the un-actioned completions this window shows. */
    private ModelList buildCompletionPopupRows(Map<ContentId, ListItem> rowsById) {
        ModelList rows = new ModelList();
        addRecentRows(rows, mUnactionedIds::contains, rowsById);
        return rows;
    }

    /**
     * Builds the full tray's rows, as desktop's main view: the downloads in progress this window
     * shows, then its recent completed and interrupted ones, acted on or not.
     */
    private ModelList buildFullTrayRows(Map<ContentId, ListItem> rowsById) {
        // The downloads in progress that this window shows, in desktop's order.
        boolean isIncognitoSelected = mIsIncognitoSelected.getAsBoolean();
        List<OfflineItem> active = new ArrayList<>(mModel.getActiveItems());
        active.removeIf(item -> !DownloadStateModel.isVisibleInWindow(item, isIncognitoSelected));
        active.sort(DownloadTrayMediator::compareActiveRows);

        // In-progress rows first, then the recent ended ones, under one cap on rows, as desktop's
        // kDefaultMaxNumItemsToShow.
        ModelList rows = new ModelList();
        Predicate<ContentId> listAll = _ -> true;
        addRows(rows, active, listAll, rowsById);
        addRecentRows(rows, listAll, rowsById);
        return rows;
    }

    /** Orders active downloads as desktop's ItemSortKey: paused last, then newest-started first. */
    private static int compareActiveRows(OfflineItem a, OfflineItem b) {
        boolean isAPaused = a.state == OfflineItemState.PAUSED;
        boolean isBPaused = b.state == OfflineItemState.PAUSED;
        if (isAPaused != isBPaused) {
            return isAPaused ? 1 : -1;
        }
        return Long.compare(b.creationTimeMs, a.creationTimeMs);
    }

    /**
     * Adds to {@code rows}, while there is room, a row for each of the model's recent ended
     * downloads that this window shows and {@code shouldList} accepts, most recently started first.
     */
    private void addRecentRows(
            ModelList rows, Predicate<ContentId> shouldList, Map<ContentId, ListItem> rowsById) {
        // Re-checked as the window may have switched between regular and incognito since.
        boolean isIncognitoSelected = mIsIncognitoSelected.getAsBoolean();
        List<OfflineItem> recent = mModel.getRecentEndedItems(isIncognitoSelected);
        markNoLongerRecentAsActioned(recent, isIncognitoSelected);
        addRows(rows, recent, shouldList, rowsById);
    }

    /**
     * Adds to {@code rows}, while there is room, a row for each of {@code items} that {@code
     * shouldList} accepts and that isn't dangerous, recording each row's download in {@code
     * rowsById}.
     */
    private void addRows(
            ModelList rows,
            List<OfflineItem> items,
            Predicate<ContentId> shouldList,
            Map<ContentId, ListItem> rowsById) {
        for (OfflineItem item : items) {
            if (rows.size() >= DownloadStateModel.MAX_RECENT_ITEMS) {
                break;
            }
            ContentId id = assumeNonNull(item.id); // The model only keeps items with ids.
            if (!shouldList.test(id) || isDangerous(item)) continue;
            ListItem row = new ListItem(DownloadTrayItemProperties.ROW_TYPE, createItemModel(item));
            rows.add(row);
            rowsById.put(id, row);
        }
    }

    /**
     * Marks un-actioned completions that no window lists any more (started over 24 hours ago or
     * past the cap) as actioned, as desktop does. The other window mode's list is checked too, to
     * keep those remembered for it, e.g. incognito ones while regular tabs show.
     *
     * @param recent The window's recent list for {@code isIncognitoSelected}.
     * @param isIncognitoSelected Whether the window is showing incognito tabs.
     */
    private void markNoLongerRecentAsActioned(
            List<OfflineItem> recent, boolean isIncognitoSelected) {
        if (mUnactionedIds.isEmpty()) {
            return;
        }
        Set<ContentId> listedIds = new ArraySet<>();
        for (OfflineItem item : recent) {
            listedIds.add(assumeNonNull(item.id));
        }
        for (OfflineItem item : mModel.getRecentEndedItems(!isIncognitoSelected)) {
            listedIds.add(assumeNonNull(item.id));
        }
        mUnactionedIds.retainAll(listedIds);
    }

    private void onOpen(OfflineItem item) {
        mUnactionedIds.remove(item.id);
        mOnActioned.run();
        mPopupController.dismiss();
        mDelegate.openItem(item);
    }

    /**
     * Whether any download is still downloading. As on desktop ({@code IsAllDone()} in {@code
     * DownloadDisplayController}), the popup waits for these so a burst of downloads ends in one
     * popup. Paused downloads, pending ones (interrupted and waiting to auto-resume, e.g. for a
     * network), dangerous ones awaiting the user's decision (which desktop treats as done) and ones
     * this window doesn't show don't hold it back.
     */
    private boolean isAnythingDownloading() {
        boolean isIncognitoSelected = mIsIncognitoSelected.getAsBoolean();
        for (OfflineItem active : mModel.getActiveItems()) {
            if (active.state == OfflineItemState.IN_PROGRESS
                    && !isDangerous(active)
                    && DownloadStateModel.isVisibleInWindow(active, isIncognitoSelected)) {
                return true;
            }
        }
        return false;
    }

    /** Whether {@code item} is dangerous and so has its own warning UI instead of the tray. */
    private static boolean isDangerous(OfflineItem item) {
        return DownloadUtils.shouldDisplayDownloadAsDangerous(item.dangerType, item.state);
    }

    private PropertyModel createItemModel(OfflineItem item) {
        PropertyModel row =
                new PropertyModel.Builder(DownloadTrayItemProperties.ALL_KEYS)
                        .with(DownloadTrayItemProperties.ITEM_ID, assertNonNull(item.id))
                        .build();
        updateRow(row, item, /* previous= */ null);
        return row;
    }

    /** Fills {@code row} from {@code item}, e.g. as it progresses, ends or is renamed. */
    private void updateRow(PropertyModel row, OfflineItem item, @Nullable OfflineItem previous) {
        boolean isActive = DownloadStateModel.isActive(item);
        row.set(DownloadTrayItemProperties.ICON, getIcon(item));
        row.set(DownloadTrayItemProperties.TITLE, item.title);
        row.set(
                DownloadTrayItemProperties.DESCRIPTION,
                isActive ? getInProgressDescription(item) : getEndedDescription(item));
        row.set(DownloadTrayItemProperties.PROGRESS, getProgress(item));
        // State-dependent properties don't change on progress ticks; previous is non-null only for
        // downloads that were active (which have nothing to open), so ended rows always refresh
        // fully, including ON_OPEN.
        if (previous != null && item.state == previous.state) return;
        row.set(DownloadTrayItemProperties.IS_IN_PROGRESS, isActive);
        row.set(DownloadTrayItemProperties.IS_PAUSED, item.state == OfflineItemState.PAUSED);
        // Only a completed download has a file to open; one in progress or interrupted doesn't.
        row.set(
                DownloadTrayItemProperties.ON_OPEN,
                item.state == OfflineItemState.COMPLETE ? _ -> onOpen(item) : null);
        row.set(DownloadTrayItemProperties.PRIMARY_ACTION, getPrimaryAction(item));
        row.set(DownloadTrayItemProperties.SECONDARY_ACTION, getSecondaryAction(item));
    }

    /**
     * Returns the row's primary action, or null for a completed download. As on the Downloads page
     * ({@code InProgressViewHolder}), a pending download pauses and an interrupted one resumes.
     */
    private @Nullable ActionButton getPrimaryAction(OfflineItem item) {
        if (item.state == OfflineItemState.IN_PROGRESS || item.state == OfflineItemState.PENDING) {
            return mPauseAction;
        }
        if (item.state == OfflineItemState.PAUSED) {
            return mResumeAction;
        }
        if (item.state == OfflineItemState.INTERRUPTED) {
            return mRetryAction;
        }
        // OfflineItemState.COMPLETE (or any other ended state): no actions.
        return null;
    }

    /** Returns the row's secondary action (cancel), or null for a completed download. */
    private @Nullable ActionButton getSecondaryAction(OfflineItem item) {
        if (DownloadStateModel.isActive(item) || item.state == OfflineItemState.INTERRUPTED) {
            return mCancelAction;
        }
        // OfflineItemState.COMPLETE (or any other ended state): no actions.
        return null;
    }

    /**
     * Returns the row's icon: the file type's, or for an interrupted download the download-off
     * icon, as desktop's {@code IconAndColorForDownloadOff()}.
     */
    private static @DrawableRes int getIcon(OfflineItem item) {
        if (item.state == OfflineItemState.INTERRUPTED) {
            return R.drawable.ic_file_download_off_24dp;
        }
        // OfflineItemState.COMPLETE, or an active download.
        return DownloadIconUtils.getIconForItem(item);
    }

    /**
     * Returns the progress bar's value: how much has been received, from 0 to 100, or {@link
     * DownloadTrayItemProperties#PROGRESS_INDETERMINATE} with no known size, as desktop does.
     */
    private static int getProgress(OfflineItem item) {
        if (item.totalSizeBytes > 0) {
            // Clamped in case more arrives than the announced size.
            float progress =
                    MathUtils.clamp((float) item.receivedBytes / item.totalSizeBytes, 0f, 1f);
            return Math.round(progress * 100);
        }
        // Unknown size: as on the Downloads page, the bar only animates while downloading.
        return item.state == OfflineItemState.IN_PROGRESS
                ? DownloadTrayItemProperties.PROGRESS_INDETERMINATE
                : 0;
    }

    /**
     * Returns an active download's status line, e.g. "65.00 MB / 100.00 MB • 15 secs left", as
     * desktop's {@code BubbleStatusTextBuilder}, in the Downloads page's wording.
     */
    private String getInProgressDescription(OfflineItem item) {
        boolean isTotalSizeKnown = item.totalSizeBytes > 0;
        // E.g. "2.15 MB / 4.30 MB"; "1.50 MB / ?" with no known total; "Downloading…" with no known
        // total and nothing received yet.
        String progressText =
                StringUtils.getProgressTextForUi(
                        new OfflineItem.Progress(
                                item.receivedBytes,
                                isTotalSizeKnown ? item.totalSizeBytes : null,
                                OfflineItemProgressUnit.BYTES));
        @Nullable String status = getInProgressStatus(item);

        // Downloading with no time left known yet: the progress alone.
        if (status == null) {
            return progressText;
        }
        // No sizes yet: the status alone, e.g. "Paused" rather than "Downloading… • Paused".
        boolean hasNoSizesYet = !isTotalSizeKnown && item.receivedBytes <= 0;
        if (hasNoSizesYet) {
            return status;
        }
        // E.g. "2.15 MB / 4.30 MB • Paused".
        return formatStatus(progressText, status);
    }

    /** Returns what follows an active download's sizes: its state or time left, or null if none. */
    private @Nullable String getInProgressStatus(OfflineItem item) {
        if (item.state == OfflineItemState.PAUSED) {
            return mContext.getString(R.string.download_tray_item_status_paused);
        }
        if (item.state == OfflineItemState.PENDING) {
            return mContext.getString(R.string.download_tray_item_status_pending);
        }
        // OfflineItemState.IN_PROGRESS.
        return item.timeRemainingMs > 0
                ? StringUtils.timeLeftForUi(mContext, item.timeRemainingMs)
                : null;
    }

    /**
     * Returns an ended download's status line: "Download failed" for an interrupted download,
     * otherwise e.g. "4.30 MB • 3 minutes ago", as desktop's {@code BubbleStatusTextBuilder}.
     */
    private String getEndedDescription(OfflineItem item) {
        if (item.state == OfflineItemState.INTERRUPTED) {
            return mContext.getString(R.string.download_tray_item_status_failed);
        }
        // OfflineItemState.COMPLETE.
        long bytes = item.totalSizeBytes > 0 ? item.totalSizeBytes : item.receivedBytes;
        return formatStatus(
                DownloadUtils.getStringForBytes(mContext, bytes), getCompletedState(item));
    }

    /**
     * Joins {@code sizes} and {@code status} into a status line, e.g. "4.30 MB • Done". As on
     * desktop, sizes that read left-to-right stay so in right-to-left locales (e.g. "4.30 MB", not
     * "MB 4.30"), while ones whose units are translated to a right-to-left script are left as is.
     */
    private String formatStatus(String sizes, CharSequence status) {
        String wrappedSizes =
                BidiFormatter.getInstance()
                        .unicodeWrap(sizes, TextDirectionHeuristicsCompat.FIRSTSTRONG_LTR);
        return mContext.getString(
                R.string.download_tray_item_status_message_with_separator, wrappedSizes, status);
    }

    /**
     * Returns how long ago {@code item} completed: "Done" within a minute, then e.g. "3 minutes
     * ago".
     */
    private CharSequence getCompletedState(OfflineItem item) {
        long nowMs = TimeUtils.currentTimeMillis();
        // Offline items may have no completion time, and a clock set back gives a negative age.
        if (item.completionTimeMs <= 0
                || nowMs - item.completionTimeMs < DateUtils.MINUTE_IN_MILLIS) {
            return mContext.getString(R.string.download_tray_item_status_done);
        }
        return DateUtils.getRelativeTimeSpanString(
                item.completionTimeMs, nowMs, DateUtils.MINUTE_IN_MILLIS, /* flags= */ 0);
    }
}
