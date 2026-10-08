// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.os.Handler;
import android.os.Looper;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.MathUtils;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.download.DownloadToolbarButtonState.IconState;
import org.chromium.components.offline_items_collection.ContentId;
import org.chromium.components.offline_items_collection.OfflineContentProvider;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.components.offline_items_collection.OfflineItemState;
import org.chromium.components.offline_items_collection.UpdateDelta;

import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.TimeUnit;

/**
 * Derives the state of the download toolbar button from download state reported by an {@link
 * OfflineContentProvider}. Mirrors the desktop download bubble; see {@code
 * DownloadDisplayController} in {@code chrome/browser/download/bubble/} as the behavioural
 * reference.
 *
 * <p>The button is shown while any trackable item is active (in progress, pending or paused). Once
 * the last active item reaches a terminal state it stays visible for {@link #AUTO_HIDE_DELAY_MS}
 * within the current session, then hides. If every tracked item is removed before then, it hides
 * immediately. Transient and suggested items are ignored, as in {@code
 * DownloadMessageUiControllerImpl}. Items from all profiles are tracked, since the toolbar is
 * shared by both tab models within an activity.
 *
 * <p>TODO(crbug.com/569024304): Derive state from a shared download-state model (the Android
 * analogue of desktop's {@code DownloadBubbleUpdateService}) once the download tray needs it.
 */
@NullMarked
public class DownloadToolbarButtonController
        implements OfflineContentProvider.Observer, Destroyable {
    /**
     * How long the button stays visible after the last active download ends. Held in memory only,
     * so it does not survive a process restart.
     */
    @VisibleForTesting static final long AUTO_HIDE_DELAY_MS = TimeUnit.MINUTES.toMillis(60);

    private final OfflineContentProvider mProvider;
    private final Handler mHandler = new Handler(Looper.getMainLooper());
    private final SettableNonNullObservableSupplier<DownloadToolbarButtonState> mStateSupplier =
            ObservableSuppliers.createNonNull(DownloadToolbarButtonState.HIDDEN);

    /** Active items keyed by id, holding the latest data for each so progress can be aggregated. */
    private final Map<ContentId, OfflineItem> mActiveItems = new HashMap<>();

    /**
     * Items that ended during this session and have not been removed since. While nothing is active
     * these are what the lingering button surfaces; once all are removed it hides.
     */
    private final Set<ContentId> mEndedItems = new HashSet<>();

    private final Runnable mAutoHideRunnable = this::onAutoHideTimeout;

    /**
     * Ids reported by observer callbacks before the initial {@link
     * OfflineContentProvider#getAllItems} snapshot arrives; callbacks are fresher than the snapshot
     * and take precedence. Null once the snapshot has been processed.
     */
    private @Nullable Set<ContentId> mIdsUpdatedBeforeSnapshot = new HashSet<>();

    private boolean mIsDestroyed;

    /**
     * Creates a controller tracking download state from {@code provider}.
     *
     * @param provider The provider to observe for download state changes.
     */
    public DownloadToolbarButtonController(OfflineContentProvider provider) {
        mProvider = provider;
        mProvider.addObserver(this);
        // Seed from items already in flight (e.g. a download started in another window).
        mProvider.getAllItems(this::onAllItemsRetrieved);
    }

    /** Returns a supplier of the state the download toolbar button should currently display. */
    public NonNullObservableSupplier<DownloadToolbarButtonState> getStateSupplier() {
        return mStateSupplier;
    }

    /**
     * Stops observing downloads, cancels any pending auto-hide and resets the supplier to {@link
     * DownloadToolbarButtonState#HIDDEN} so observers that outlive this controller do not keep a
     * stale state.
     */
    @Override
    public void destroy() {
        if (mIsDestroyed) {
            return;
        }
        mIsDestroyed = true;
        cancelAutoHide();
        mProvider.removeObserver(this);
        mActiveItems.clear();
        mEndedItems.clear();
        mIdsUpdatedBeforeSnapshot = null;
        // With both collections cleared this publishes HIDDEN. Reset rather than destroy the
        // supplier: destroying would null the value of a non-null supplier, which is unsafe for
        // any observer that reads it after this point.
        publishState();
    }

    // OfflineContentProvider.Observer implementation:

    @Override
    public void onItemsAdded(List<OfflineItem> items) {
        if (mIsDestroyed) {
            return;
        }
        for (OfflineItem item : items) {
            recordUpdateBeforeSnapshot(item.id);
            onItemChanged(item);
        }
    }

    @Override
    public void onItemUpdated(OfflineItem item, @Nullable UpdateDelta updateDelta) {
        if (mIsDestroyed) {
            return;
        }
        recordUpdateBeforeSnapshot(item.id);
        onItemChanged(item);
    }

    @Override
    public void onItemRemoved(ContentId id) {
        if (mIsDestroyed) {
            return;
        }
        recordUpdateBeforeSnapshot(id);
        boolean wasActive = mActiveItems.remove(id) != null;
        boolean wasEnded = mEndedItems.remove(id);
        if (!wasActive && !wasEnded) {
            return;
        }
        if (mActiveItems.isEmpty() && mEndedItems.isEmpty()) {
            // Nothing is left for the button to surface, so do not linger.
            cancelAutoHide();
        } else if (mActiveItems.isEmpty() && wasActive) {
            // Last active item removed while ended items remain: start their linger window. If
            // an ended item was removed instead, the running window is left untouched.
            scheduleAutoHide();
        }
        publishState();
    }

    private void recordUpdateBeforeSnapshot(@Nullable ContentId id) {
        if (mIdsUpdatedBeforeSnapshot != null && id != null) {
            mIdsUpdatedBeforeSnapshot.add(id);
        }
    }

    private void onAllItemsRetrieved(List<OfflineItem> items) {
        if (mIsDestroyed || mIdsUpdatedBeforeSnapshot == null) {
            return;
        }
        Set<ContentId> updatedBeforeSnapshot = mIdsUpdatedBeforeSnapshot;
        mIdsUpdatedBeforeSnapshot = null;

        // Only items still in flight seed the button; already-ended items are ignored so the
        // post-completion window does not carry over across a restart.
        for (OfflineItem item : items) {
            if (!isTrackable(item) || !isActive(item)) {
                continue;
            }
            // An observer callback already reported a newer state (e.g. completion or removal);
            // seeding from the stale snapshot would leave the item active forever.
            if (updatedBeforeSnapshot.contains(item.id)) {
                continue;
            }
            mActiveItems.put(assumeNonNull(item.id), item); // Checked by isTrackable().
        }
        if (!mActiveItems.isEmpty()) {
            // An item that ended before the snapshot arrived may have started the auto-hide
            // window; the seeded active items now keep the button visible instead.
            cancelAutoHide();
        }
        publishState();
    }

    private void onItemChanged(OfflineItem item) {
        if (!isTrackable(item)) {
            return;
        }
        ContentId id = assumeNonNull(item.id); // Checked by isTrackable().
        if (isActive(item)) {
            mActiveItems.put(id, item);
            // A resumed or retried item is active again rather than ended.
            mEndedItems.remove(id);
            cancelAutoHide();
            publishState();
            return;
        }

        // Only items observed active this session count as ended here; terminal updates for
        // anything else (e.g. a pre-existing download being opened) are not tracked.
        boolean wasActive = mActiveItems.remove(id) != null;
        if (!wasActive) {
            return;
        }
        mEndedItems.add(id);
        if (mActiveItems.isEmpty()) {
            // The last active item reached a terminal state. Regardless of outcome, keep the
            // button visible for the window so the user can still reach the item, matching
            // desktop.
            scheduleAutoHide();
        }
        publishState();
    }

    private void scheduleAutoHide() {
        cancelAutoHide();
        mHandler.postDelayed(mAutoHideRunnable, AUTO_HIDE_DELAY_MS);
    }

    private void cancelAutoHide() {
        mHandler.removeCallbacks(mAutoHideRunnable);
    }

    private void onAutoHideTimeout() {
        if (mIsDestroyed) {
            return;
        }
        // Any item becoming active cancels the pending auto-hide, so the timer can only fire with
        // nothing active.
        assert mActiveItems.isEmpty() : "Auto-hide fired while downloads are active";
        // The window for the ended items has elapsed; they no longer influence visibility.
        mEndedItems.clear();
        publishState();
    }

    /** Recomputes the button state and notifies observers if it changed. */
    private void publishState() {
        DownloadToolbarButtonState state = computeState();
        if (!state.equals(mStateSupplier.get())) {
            mStateSupplier.set(state);
        }
    }

    /**
     * Derives the button state purely from {@link #mActiveItems} and {@link #mEndedItems}, so it
     * can be recomputed after any change without tracking what changed. Mirrors {@code
     * DownloadDisplayController::UpdateToolbarButtonState} on desktop.
     *
     * <ul>
     *   <li>Nothing active and nothing ended: {@link DownloadToolbarButtonState#HIDDEN}.
     *   <li>Nothing active but some items ended: the button lingers for the auto-hide window,
     *       showing the inactive complete icon.
     *   <li>Anything active: the progress icon, active unless every item is paused, with the count
     *       and aggregate progress of the active items.
     * </ul>
     */
    private DownloadToolbarButtonState computeState() {
        if (mActiveItems.isEmpty() && mEndedItems.isEmpty()) {
            return DownloadToolbarButtonState.HIDDEN;
        }

        int downloadCount = mActiveItems.size();
        if (downloadCount == 0) {
            // Lingering after the last active item ended. The icon is inactive; the active window
            // for an unactioned completion is handled separately.
            return new DownloadToolbarButtonState(
                    /* shouldShow= */ true,
                    IconState.COMPLETE,
                    /* isActive= */ false,
                    /* downloadCount= */ 0,
                    /* progressPercent= */ 0,
                    /* progressCertain= */ true);
        }

        // Aggregate progress as DownloadBubbleUpdateService::CacheManager::GetProgressInfo does:
        // items whose total size is unknown make the overall progress uncertain and are excluded
        // from the percentage.
        int pausedCount = 0;
        long receivedBytes = 0;
        long totalBytes = 0;
        boolean progressCertain = true;
        for (OfflineItem item : mActiveItems.values()) {
            if (item.state == OfflineItemState.PAUSED) {
                pausedCount++;
            }
            if (item.totalSizeBytes <= 0) {
                progressCertain = false;
                continue;
            }
            receivedBytes += item.receivedBytes;
            totalBytes += item.totalSizeBytes;
        }
        int progressPercent = 0;
        if (totalBytes > 0) {
            progressPercent = (int) MathUtils.clamp(receivedBytes * 100 / totalBytes, 0, 100);
        }
        return new DownloadToolbarButtonState(
                /* shouldShow= */ true,
                IconState.PROGRESS,
                // Only when every active item is paused does the icon go inactive, as on desktop.
                /* isActive= */ pausedCount < downloadCount,
                downloadCount,
                progressPercent,
                progressCertain);
    }

    /**
     * Excludes transient items (e.g. Background Fetch jobs, which only surface as notifications)
     * and suggested items (e.g. prefetched content), so only user-initiated downloads count.
     */
    private static boolean isTrackable(OfflineItem item) {
        return item.id != null && !item.isTransient && !item.isSuggested;
    }

    private static boolean isActive(OfflineItem item) {
        switch (item.state) {
            case OfflineItemState.IN_PROGRESS:
            case OfflineItemState.PENDING:
            case OfflineItemState.PAUSED:
                return true;
            default:
                return false;
        }
    }
}
