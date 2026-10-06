// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.os.Handler;
import android.os.Looper;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.offline_items_collection.ContentId;
import org.chromium.components.offline_items_collection.OfflineContentProvider;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.components.offline_items_collection.OfflineItemState;
import org.chromium.components.offline_items_collection.UpdateDelta;

import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.concurrent.TimeUnit;

/**
 * Drives the visibility of the download toolbar button from download state reported by an {@link
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
     * How long the button stays visible after the last active download ends. Matches {@code
     * kToolbarIconVisibilityTimeInterval} of the desktop download bubble. Held in memory only, so
     * it does not survive a process restart.
     */
    @VisibleForTesting static final long AUTO_HIDE_DELAY_MS = TimeUnit.MINUTES.toMillis(60);

    private final OfflineContentProvider mProvider;
    private final Handler mHandler = new Handler(Looper.getMainLooper());
    private final SettableNonNullObservableSupplier<Boolean> mShouldShowSupplier =
            ObservableSuppliers.createNonNull(/* initialValue= */ false);
    private final Set<ContentId> mActiveItems = new HashSet<>();

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

    /** Returns a supplier of whether the download toolbar button should currently be shown. */
    public NonNullObservableSupplier<Boolean> getShouldShowSupplier() {
        return mShouldShowSupplier;
    }

    /**
     * Stops observing downloads, cancels any pending auto-hide and resets the supplier to {@code
     * false} so observers that outlive this controller do not keep a stale "show" state.
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
        // Reset rather than destroy the supplier: destroying would null the value of a non-null
        // supplier, which is unsafe for any observer that reads it after this point.
        mShouldShowSupplier.set(false);
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
        boolean wasActive = mActiveItems.remove(id);
        boolean wasEnded = mEndedItems.remove(id);
        if ((!wasActive && !wasEnded) || !mActiveItems.isEmpty()) {
            return;
        }
        if (mEndedItems.isEmpty()) {
            // Nothing is left for the button to surface, so do not linger.
            hide();
        } else if (wasActive) {
            // Last active item removed while ended items remain: start their linger window. If
            // an ended item was removed instead, the running window is left untouched.
            scheduleAutoHide();
        }
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
            mActiveItems.add(item.id);
        }
        if (!mActiveItems.isEmpty()) {
            show();
        }
    }

    private void onItemChanged(OfflineItem item) {
        if (!isTrackable(item)) {
            return;
        }
        ContentId id = assumeNonNull(item.id); // Checked by isTrackable().
        if (isActive(item)) {
            mActiveItems.add(id);
            // A resumed or retried item is active again rather than ended.
            mEndedItems.remove(id);
            show();
            return;
        }

        // Only items observed active this session count as ended here; terminal updates for
        // anything else (e.g. a pre-existing download being opened) are not tracked.
        boolean wasActive = mActiveItems.remove(id);
        if (!wasActive) {
            return;
        }
        mEndedItems.add(id);
        if (!mActiveItems.isEmpty()) {
            return;
        }
        // The last active item reached a terminal state. Regardless of outcome, keep the button
        // visible for the window so the user can still reach the item, matching desktop.
        scheduleAutoHide();
    }

    private void show() {
        cancelAutoHide();
        mShouldShowSupplier.set(true);
    }

    private void hide() {
        cancelAutoHide();
        mShouldShowSupplier.set(false);
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
        // show() cancels any pending auto-hide, so the timer can only fire with nothing active.
        assert mActiveItems.isEmpty() : "Auto-hide fired while downloads are active";
        // The window for the ended items has elapsed; they no longer influence visibility.
        mEndedItems.clear();
        mShouldShowSupplier.set(false);
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
