// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.tray;

import android.content.Context;
import android.view.View;

import androidx.annotation.DrawableRes;
import androidx.annotation.VisibleForTesting;
import androidx.core.text.BidiFormatter;
import androidx.core.text.TextDirectionHeuristicsCompat;

import org.chromium.base.TimeUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.download.DownloadStateModel;
import org.chromium.chrome.browser.download.R;
import org.chromium.components.browser_ui.util.DownloadUtils;
import org.chromium.components.offline_items_collection.ContentId;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.components.offline_items_collection.OfflineItemFilter;
import org.chromium.components.offline_items_collection.OfflineItemState;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.function.BooleanSupplier;
import java.util.function.Supplier;

/**
 * Decides when the download tray shows a download and builds the models for its rows. Mirrors the
 * desktop download bubble's partial view: a download this window saw in progress that completes is
 * shown in a popup anchored to the toolbar button.
 */
@NullMarked
class DownloadTrayMediator implements DownloadStateModel.Observer {
    /** Shows and dismisses the tray popup on the mediator's behalf. */
    interface PopupController {
        /** Shows {@code row} in a popup anchored to {@code anchor}, replacing any shown popup. */
        void showCompletion(View anchor, PropertyModel row);

        /** Dismisses the popup, if showing. */
        void dismiss();
    }

    /**
     * Minimum time between two completion popups, so a burst of completions doesn't keep popping
     * up. Mirrors desktop's {@code kShowPartialViewMinInterval}.
     */
    @VisibleForTesting static final long MIN_SHOW_INTERVAL_MS = 15_000;

    private final Context mContext;
    private final DownloadStateModel mModel;
    private final Supplier<@Nullable View> mAnchorSupplier;
    private final BooleanSupplier mIsWindowFocused;
    private final Runnable mOnActioned;
    private final DownloadTrayDelegate mDelegate;
    private final PopupController mPopupController;

    /** The download in the showing popup, or null if no popup is showing. */
    private @Nullable ContentId mShownId;

    /**
     * When the last popup was shown. Starts one interval in the past so the first completion always
     * shows ({@link TimeUtils#elapsedRealtimeMillis()} is never negative).
     */
    private long mLastShownTimeMs = -MIN_SHOW_INTERVAL_MS;

    /**
     * @param context The context used to resolve resources.
     * @param model The model to observe for download state changes.
     * @param anchorSupplier Supplies the view to anchor popups to, or null if unavailable.
     * @param isWindowFocused Whether this window has focus; only the focused window shows popups.
     * @param onActioned Invoked when the user acts on a download from the tray.
     * @param delegate Performs actions that need dependencies outside the download module.
     * @param popupController Shows and dismisses the popup.
     */
    DownloadTrayMediator(
            Context context,
            DownloadStateModel model,
            Supplier<@Nullable View> anchorSupplier,
            BooleanSupplier isWindowFocused,
            Runnable onActioned,
            DownloadTrayDelegate delegate,
            PopupController popupController) {
        mContext = context;
        mModel = model;
        mAnchorSupplier = anchorSupplier;
        mIsWindowFocused = isWindowFocused;
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
        mShownId = null;
    }

    // DownloadStateModel.Observer implementation:

    @Override
    public void onItemChanged(OfflineItem item, @Nullable OfflineItem previous) {
        // Only a download this window saw in progress that has just completed is shown.
        boolean completed =
                previous != null
                        && DownloadStateModel.isActive(previous)
                        && item.state == OfflineItemState.COMPLETE;
        if (!completed
                || DownloadUtils.shouldDisplayDownloadAsDangerous(item.dangerType, item.state)
                || !mIsWindowFocused.getAsBoolean()) {
            return;
        }

        long now = TimeUtils.elapsedRealtimeMillis();
        if (now - mLastShownTimeMs < MIN_SHOW_INTERVAL_MS) return;

        View anchor = mAnchorSupplier.get();
        if (anchor == null || !anchor.isShown()) return;

        mLastShownTimeMs = now;
        // Showing replaces any previous popup, whose dismissal resets mShownId, so set it after.
        mPopupController.showCompletion(anchor, createCompletedItemModel(item));
        mShownId = item.id;
    }

    @Override
    public void onItemRemoved(ContentId id, @Nullable OfflineItem previous) {
        // E.g. deleted from the Downloads page while its popup is showing.
        if (id.equals(mShownId)) mPopupController.dismiss();
    }

    private void onOpen(OfflineItem item) {
        mOnActioned.run();
        mPopupController.dismiss();
        mDelegate.openItem(item);
    }

    private PropertyModel createCompletedItemModel(OfflineItem item) {
        return new PropertyModel.Builder(DownloadTrayItemProperties.ALL_KEYS)
                .with(DownloadTrayItemProperties.ICON, getIconForItem(item))
                .with(DownloadTrayItemProperties.TITLE, item.title)
                .with(DownloadTrayItemProperties.DESCRIPTION, getCompletedDescription(item))
                .with(DownloadTrayItemProperties.ON_OPEN, v -> onOpen(item))
                .build();
    }

    /**
     * Returns the status line for a completed download, e.g. "Download complete • 4.30 MB".
     *
     * <p>TODO(crbug.com/574064718): Update the strings to match the desktop download bubble's "4.30
     * MB • Done" ({@code StatusTextBuilderUtils::GetCompletedTotalSizeString}) once they can be
     * added. Reuses the download notification's string until then.
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
        return mContext.getString(R.string.download_notification_completed_with_size, size);
    }

    /**
     * Returns the file-type icon for {@code item}. Mirrors {@code UiUtils.getIconForItem} on the
     * Downloads page for the items the tray can show; dangerous items never reach the tray.
     *
     * <p>TODO(crbug.com/574064718): Share the Downloads page helper instead of mirroring it.
     */
    @VisibleForTesting
    static @DrawableRes int getIconForItem(OfflineItem item) {
        switch (item.filter) {
            case OfflineItemFilter.PAGE:
                return R.drawable.ic_globe_24dp;
            case OfflineItemFilter.VIDEO:
                return R.drawable.ic_videocam_fill_24dp;
            case OfflineItemFilter.AUDIO:
                return R.drawable.ic_music_note_24dp;
            case OfflineItemFilter.IMAGE:
                return R.drawable.ic_drive_image_24dp;
            default:
                return R.drawable.ic_drive_file_24dp;
        }
    }
}
