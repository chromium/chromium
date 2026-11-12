// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.tray;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.util.ArrayMap;
import android.util.ArraySet;
import android.view.View;

import androidx.annotation.VisibleForTesting;
import androidx.core.text.BidiFormatter;
import androidx.core.text.TextDirectionHeuristicsCompat;

import org.chromium.base.TimeUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.download.DownloadIconUtils;
import org.chromium.chrome.browser.download.DownloadStateModel;
import org.chromium.chrome.browser.download.R;
import org.chromium.chrome.browser.download.tray.DownloadTrayItemProperties.RowType;
import org.chromium.components.browser_ui.util.DownloadUtils;
import org.chromium.components.offline_items_collection.ContentId;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.components.offline_items_collection.OfflineItemState;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.Map;
import java.util.Set;
import java.util.concurrent.TimeUnit;
import java.util.function.BooleanSupplier;
import java.util.function.Supplier;

/**
 * Decides when the download tray shows downloads and builds the models for its rows. Mirrors the
 * desktop download bubble's partial view: once nothing is downloading, every download this window
 * saw complete that the user hasn't acted on yet is listed in a popup anchored to the toolbar
 * button.
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

    /**
     * Downloads this window saw complete that the user hasn't acted on, as desktop's {@code
     * !WasActionedOn()}. Their data comes from {@link DownloadStateModel#getRecentCompletedItems()}
     * when shown, so ones that have aged out of it count as actioned, as on desktop. These sets and
     * maps hold at most a few dozen entries (bounded by that list), so array-backed ones suffice.
     */
    private final Set<ContentId> mUnactionedIds = new ArraySet<>();

    /** The rows in the showing popup by download; empty if no popup is showing. */
    private final Map<ContentId, ListItem> mShownRows = new ArrayMap<>();

    /** The showing popup's list, or null if no popup is showing. */
    private @Nullable ModelList mShownList;

    /**
     * Completions since the last popup, waiting to be shown once nothing is still downloading. Kept
     * so they are still shown if the last active download fails, is cancelled or is removed.
     */
    private final Set<ContentId> mPendingIds = new ArraySet<>();

    /**
     * When the last popup was shown. Starts one interval in the past so the first completion always
     * shows ({@link TimeUtils#elapsedRealtimeMillis()} is never negative).
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
    }

    // DownloadStateModel.Observer implementation:

    @Override
    public void onItemChanged(OfflineItem item, @Nullable OfflineItem previous) {
        ContentId id = assumeNonNull(item.id); // The model only reports items with ids.
        boolean completed = item.state == OfflineItemState.COMPLETE && !isDangerous(item);
        if (completed) {
            // A shown row follows its download, e.g. renamed from the Downloads page.
            ListItem shownRow = mShownRows.get(id);
            if (shownRow != null) {
                updateRow(shownRow.model, item);
            }
        } else {
            // No longer a completed download, e.g. retried or flagged as dangerous, so it is
            // neither listed nor a reason to show the popup, as if removed.
            dropCompletion(id);
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
        if (previous != null) {
            // An active download removed, e.g. cancelled, may leave nothing downloading.
            maybeShowPendingCompletions();
        }
    }

    /**
     * Drops {@code id} from the un-actioned and pending completions and takes its row out of the
     * showing popup, closing it once nothing is left.
     */
    private void dropCompletion(ContentId id) {
        mUnactionedIds.remove(id);
        mPendingIds.remove(id);
        ListItem shownRow = mShownRows.remove(id);
        if (shownRow == null || mShownList == null) return;
        mShownList.remove(shownRow);
        if (mShownRows.isEmpty()) {
            mPopupController.dismiss();
        }
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
        ModelList rows = buildUnactionedRows(rowsById);
        if (rows.isEmpty()) return;

        // Showing replaces any previous popup, whose dismissal clears the shown rows, so set them
        // after.
        if (!mPopupController.showCompletions(anchor, rows)) return;
        mLastShownTimeMs = now;
        mShownList = rows;
        mShownRows.putAll(rowsById);
    }

    /**
     * Builds rows for the un-actioned completions this window shows, most recently started first,
     * recording each row's download in {@code rowsById}. Completions no longer in the model's
     * recent list are forgotten, as desktop marks them actioned.
     */
    private ModelList buildUnactionedRows(Map<ContentId, ListItem> rowsById) {
        // Re-checked as the window may have switched between regular and incognito since.
        boolean isIncognitoSelected = mIsIncognitoSelected.getAsBoolean();
        Set<ContentId> stillRecent = new ArraySet<>();
        ModelList rows = new ModelList();
        for (OfflineItem item : mModel.getRecentCompletedItems()) {
            ContentId id = assumeNonNull(item.id); // The model only keeps items with ids.
            if (!mUnactionedIds.contains(id)) continue;
            stillRecent.add(id);
            if (DownloadStateModel.isVisibleInWindow(item, isIncognitoSelected)) {
                ListItem row = new ListItem(RowType.COMPLETED, createCompletedItemModel(item));
                rows.add(row);
                rowsById.put(id, row);
            }
        }
        mUnactionedIds.retainAll(stillRecent);
        return rows;
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

    private PropertyModel createCompletedItemModel(OfflineItem item) {
        PropertyModel row = new PropertyModel(DownloadTrayItemProperties.ALL_KEYS);
        updateRow(row, item);
        return row;
    }

    /** Fills {@code row} from {@code item}, e.g. after it was renamed while showing. */
    private void updateRow(PropertyModel row, OfflineItem item) {
        row.set(DownloadTrayItemProperties.ICON, DownloadIconUtils.getIconForItem(item));
        row.set(DownloadTrayItemProperties.TITLE, item.title);
        row.set(DownloadTrayItemProperties.DESCRIPTION, getCompletedDescription(item));
        row.set(DownloadTrayItemProperties.ON_OPEN, v -> onOpen(item));
    }

    /**
     * Returns the status line for a completed download, e.g. "4.30 MB • Done", as the desktop
     * download bubble does ({@code StatusTextBuilderUtils::GetCompletedTotalSizeString}).
     */
    private String getCompletedDescription(OfflineItem item) {
        long bytes = item.totalSizeBytes > 0 ? item.totalSizeBytes : item.receivedBytes;
        // Keep the size left-to-right (e.g. "4.30 MB", not "MB 4.30") in right-to-left locales, as
        // desktop does.
        String size =
                BidiFormatter.getInstance()
                        .unicodeWrap(
                                DownloadUtils.getStringForBytes(mContext, bytes),
                                TextDirectionHeuristicsCompat.LTR);
        return mContext.getString(
                R.string.download_tray_item_status_message_with_separator,
                size,
                mContext.getString(R.string.download_tray_item_status_done));
    }
}
