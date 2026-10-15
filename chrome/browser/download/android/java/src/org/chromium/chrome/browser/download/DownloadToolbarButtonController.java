// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.os.Handler;
import android.os.Looper;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.MathUtils;
import org.chromium.base.lifetime.DestroyChecker;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.download.DownloadToolbarButtonState.IconState;
import org.chromium.components.offline_items_collection.ContentId;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.components.offline_items_collection.OfflineItemState;

import java.util.Collection;
import java.util.HashSet;
import java.util.Set;
import java.util.concurrent.TimeUnit;

/**
 * Derives the state of the download toolbar button from the items cached by a {@link
 * DownloadStateModel}. Mirrors the desktop download bubble; see {@code DownloadDisplayController}
 * in {@code chrome/browser/download/bubble/} as the behavioural reference.
 *
 * <p>The button is shown while any trackable item is active (in progress, pending or paused). Once
 * the last active item reaches a terminal state it stays visible for {@link #AUTO_HIDE_DELAY_MS}
 * within the current session, then hides. If every tracked item is removed before then, it hides
 * immediately. After a download completes the icon is drawn in the active colour for {@link
 * #ACTIVE_AFTER_COMPLETE_MS}, or until the user acts on the button via {@link #markActioned()}.
 */
@NullMarked
public class DownloadToolbarButtonController implements DownloadStateModel.Observer, Destroyable {
    /**
     * How long the button stays visible after the last active download ends. Mirrors desktop's
     * {@code kToolbarIconVisibilityTimeInterval}. Held in memory only, so it does not survive a
     * process restart.
     */
    @VisibleForTesting static final long AUTO_HIDE_DELAY_MS = TimeUnit.MINUTES.toMillis(60);

    /**
     * How long the icon stays in the active colour after a download completes, unless the user acts
     * on the button first. Mirrors desktop's {@code kToolbarIconActiveTimeInterval}.
     */
    @VisibleForTesting static final long ACTIVE_AFTER_COMPLETE_MS = TimeUnit.MINUTES.toMillis(1);

    private final DownloadStateModel mModel;
    private final Handler mHandler = new Handler(Looper.getMainLooper());
    private final SettableNonNullObservableSupplier<DownloadToolbarButtonState> mStateSupplier =
            ObservableSuppliers.createNonNull(DownloadToolbarButtonState.HIDDEN);

    /**
     * Items that ended during this session and have not been removed since. While nothing is active
     * these are what the lingering button surfaces; once all are removed it hides.
     */
    private final Set<ContentId> mEndedItems = new HashSet<>();

    private final Runnable mAutoHideRunnable = this::onAutoHideTimeout;
    private final Runnable mInactiveRunnable = this::onInactiveTimeout;

    /**
     * Items that completed within the last {@link #ACTIVE_AFTER_COMPLETE_MS} and have not been
     * acted on or removed since; a subset of {@link #mEndedItems}. Non-empty draws the complete
     * icon in the active colour. Cleared by {@link #mInactiveRunnable} or {@link #markActioned()}.
     */
    private final Set<ContentId> mUnactionedCompletions = new HashSet<>();

    private final DestroyChecker mDestroyChecker = new DestroyChecker();

    /**
     * Creates a controller deriving the button state from {@code model}.
     *
     * @param model The model to observe for download state changes. Must outlive this controller.
     */
    public DownloadToolbarButtonController(DownloadStateModel model) {
        mModel = model;
        mModel.addObserver(this);
        // The model may already hold active items, e.g. if its initial snapshot was delivered
        // synchronously before this controller attached.
        publishState();
    }

    /** Returns a supplier of the state the download toolbar button should currently display. */
    public NonNullObservableSupplier<DownloadToolbarButtonState> getStateSupplier() {
        return mStateSupplier;
    }

    /**
     * Records that the user has acted on the button (e.g. opened the downloads UI from it), which
     * ends the active colour window of any recent completion.
     */
    public void markActioned() {
        if (mDestroyChecker.isDestroyed() || mUnactionedCompletions.isEmpty()) {
            return;
        }
        mHandler.removeCallbacks(mInactiveRunnable);
        mUnactionedCompletions.clear();
        publishState();
    }

    /**
     * Stops observing the model, cancels any pending timers and resets the supplier to {@link
     * DownloadToolbarButtonState#HIDDEN} so observers that outlive this controller do not keep a
     * stale state.
     */
    @Override
    public void destroy() {
        mDestroyChecker.destroy();
        cancelAutoHide();
        mHandler.removeCallbacks(mInactiveRunnable);
        mModel.removeObserver(this);
        mEndedItems.clear();
        mUnactionedCompletions.clear();
        // Reset rather than destroy the supplier: destroying would null the value of a non-null
        // supplier, which is unsafe for any observer that reads it after this point.
        publishState();
    }

    // DownloadStateModel.Observer implementation:

    @Override
    public void onItemChanged(OfflineItem item, @Nullable OfflineItem previous) {
        ContentId id = assumeNonNull(item.id); // The model only reports trackable items.
        if (DownloadStateModel.isActive(item)) {
            // A resumed or retried item is active again rather than ended.
            mEndedItems.remove(id);
            if (mUnactionedCompletions.remove(id) && mUnactionedCompletions.isEmpty()) {
                mHandler.removeCallbacks(mInactiveRunnable);
            }
            // Any active item keeps the button visible, including one seeded from the model's
            // initial snapshot after an earlier item ended and started the auto-hide window.
            cancelAutoHide();
            publishState();
            return;
        }

        // Only items observed active this session count as ended here; terminal updates for
        // anything else (e.g. a pre-existing download being opened) are not tracked.
        boolean wasActive = previous != null && DownloadStateModel.isActive(previous);
        if (!wasActive) {
            return;
        }
        mEndedItems.add(id);
        if (item.state == OfflineItemState.COMPLETE) {
            // Only a successful completion draws the icon in the active colour for a while. The
            // other terminal states (CANCELLED, FAILED, INTERRUPTED) still keep the button visible
            // via mEndedItems but do not call attention to themselves.
            mUnactionedCompletions.add(id);
            mHandler.removeCallbacks(mInactiveRunnable);
            mHandler.postDelayed(mInactiveRunnable, ACTIVE_AFTER_COMPLETE_MS);
        }
        if (mModel.getActiveItems().isEmpty()) {
            // The last active item reached a terminal state. Regardless of outcome, keep the
            // button visible for the window so the user can still reach the item, matching
            // desktop.
            scheduleAutoHide();
        }
        publishState();
    }

    @Override
    public void onItemRemoved(ContentId id, @Nullable OfflineItem previous) {
        boolean wasActive = previous != null && DownloadStateModel.isActive(previous);
        boolean wasEnded = mEndedItems.remove(id);
        if (mUnactionedCompletions.remove(id) && mUnactionedCompletions.isEmpty()) {
            mHandler.removeCallbacks(mInactiveRunnable);
        }
        if (!wasActive && !wasEnded) {
            return;
        }
        boolean nothingActive = mModel.getActiveItems().isEmpty();
        if (nothingActive && mEndedItems.isEmpty()) {
            // Nothing is left for the button to surface, so do not linger.
            cancelAutoHide();
        } else if (nothingActive && wasActive) {
            // Last active item removed while ended items remain: start their linger window. If
            // an ended item was removed instead, the running window is left untouched.
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

    private void onInactiveTimeout() {
        if (mDestroyChecker.isDestroyed()) {
            return;
        }
        mUnactionedCompletions.clear();
        publishState();
    }

    private void onAutoHideTimeout() {
        if (mDestroyChecker.isDestroyed()) {
            return;
        }
        // Any item becoming active cancels the pending auto-hide, so the timer can only fire with
        // nothing active.
        assert mModel.getActiveItems().isEmpty() : "Auto-hide fired while downloads are active";
        // The window for the ended items has elapsed; they no longer influence visibility.
        mEndedItems.clear();
        publishState();
    }

    /** Recomputes the button state and notifies observers if it changed. */
    private void publishState() {
        // The model may outlive this controller, so a destroyed controller publishes HIDDEN
        // rather than deriving a state from the model's items.
        DownloadToolbarButtonState state =
                mDestroyChecker.isDestroyed() ? DownloadToolbarButtonState.HIDDEN : computeState();
        if (!state.equals(mStateSupplier.get())) {
            mStateSupplier.set(state);
        }
    }

    /**
     * Derives the button state purely from the model's active items, {@link #mEndedItems} and
     * {@link #mUnactionedCompletions}, so it can be recomputed after any change without tracking
     * what changed. Mirrors {@code DownloadDisplayController::UpdateToolbarButtonState} on desktop.
     *
     * <ul>
     *   <li>Nothing active and nothing ended: {@link DownloadToolbarButtonState#HIDDEN}.
     *   <li>Nothing active but some items ended: the button lingers for the auto-hide window,
     *       showing the complete icon, active only while a recent completion is unactioned.
     *   <li>Anything active: the progress icon, active unless every item is paused, with the count
     *       and aggregate progress of the active items.
     * </ul>
     */
    private DownloadToolbarButtonState computeState() {
        Collection<OfflineItem> activeItems = mModel.getActiveItems();
        if (activeItems.isEmpty() && mEndedItems.isEmpty()) {
            return DownloadToolbarButtonState.HIDDEN;
        }

        int downloadCount = activeItems.size();
        if (downloadCount == 0) {
            // Lingering after the last active item ended.
            return new DownloadToolbarButtonState(
                    /* shouldShow= */ true,
                    IconState.COMPLETE,
                    /* isActive= */ !mUnactionedCompletions.isEmpty(),
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
        for (OfflineItem item : activeItems) {
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
}
