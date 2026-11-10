// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import static org.chromium.build.NullUtil.assumeNonNull;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.CallbackController;
import org.chromium.base.ObserverList;
import org.chromium.base.TimeUtils;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.offline_items_collection.ContentId;
import org.chromium.components.offline_items_collection.OfflineContentProvider;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.components.offline_items_collection.OfflineItemState;
import org.chromium.components.offline_items_collection.UpdateDelta;

import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.TimeUnit;

/**
 * Observes an {@link OfflineContentProvider} once and caches the download items that the download
 * toolbar UI cares about, so several consumers can share one observer and one item cache. The
 * Android analogue of desktop's {@code DownloadBubbleUpdateService}.
 *
 * <p>Only trackable items are reported: transient items (e.g. Background Fetch jobs, which only
 * surface as notifications) and suggested items (e.g. prefetched content) are ignored, as in {@code
 * DownloadMessageUiControllerImpl}. Items from all profiles are tracked.
 *
 * <p>Two sets of items are kept: the active ones, which drive the toolbar button and are reported
 * to observers, and the completed ones (including those already complete when the model was
 * created), which back the download tray's recent list via {@link #getRecentCompletedItems()} and
 * never affect the button.
 */
@NullMarked
public class DownloadStateModel implements OfflineContentProvider.Observer, Destroyable {
    /**
     * How far back {@link #getRecentCompletedItems()} reaches, measured from when a download
     * started as on desktop ({@code kShowItemInBubbleDuration} and {@code GetItemStartTime()} in
     * {@code DownloadBubbleUpdateService}).
     */
    @VisibleForTesting static final long RECENT_COMPLETED_WINDOW_MS = TimeUnit.HOURS.toMillis(24);

    /**
     * The most items {@link #getRecentCompletedItems()} returns. Mirrors the desktop download
     * bubble's {@code kDefaultMaxNumItemsToShow}.
     */
    @VisibleForTesting static final int MAX_RECENT_COMPLETED_ITEMS = 30;

    /** Notified synchronously after the cache has been updated. */
    public interface Observer {
        /**
         * A trackable item was added or updated. Items already active when the initial {@link
         * OfflineContentProvider#getAllItems} snapshot arrives are reported here too, with a null
         * {@code previous}; observers added afterwards should read {@link
         * DownloadStateModel#getActiveItems()}. Items already complete in the snapshot are not
         * reported.
         *
         * @param item The latest data for the item.
         * @param previous The item's cached active data before this change, or null if it was not
         *     active.
         */
        void onItemChanged(OfflineItem item, @Nullable OfflineItem previous);

        /**
         * An item was removed.
         *
         * @param id The id of the removed item.
         * @param previous The item's cached data before removal, or null if it was not cached.
         */
        void onItemRemoved(ContentId id, @Nullable OfflineItem previous);
    }

    private final OfflineContentProvider mProvider;
    private final ObserverList<Observer> mObservers = new ObserverList<>();

    /** Active items keyed by id, holding the latest data for each. */
    private final Map<ContentId, OfflineItem> mActiveItems = new HashMap<>();

    private final Collection<OfflineItem> mActiveItemsView =
            Collections.unmodifiableCollection(mActiveItems.values());

    /**
     * Completed items keyed by id, holding the latest data for each, whether they completed during
     * this session or were already complete in the initial snapshot.
     */
    private final Map<ContentId, OfflineItem> mCompletedItems = new HashMap<>();

    /**
     * Ids reported by observer callbacks before the initial {@link
     * OfflineContentProvider#getAllItems} snapshot arrives; callbacks are fresher than the snapshot
     * and take precedence. Null once the snapshot has been processed.
     */
    private @Nullable Set<ContentId> mIdsUpdatedBeforeSnapshot = new HashSet<>();

    /** Cancels the pending snapshot request on destroy; also asserts destroy runs once. */
    private final CallbackController mCallbackController = new CallbackController();

    /**
     * Creates a model tracking download state from {@code provider}.
     *
     * @param provider The provider to observe for download state changes.
     */
    public DownloadStateModel(OfflineContentProvider provider) {
        mProvider = provider;
        mProvider.addObserver(this);
        // Seed from items already in flight (e.g. a download started in another window).
        mProvider.getAllItems(mCallbackController.makeCancelable(this::onAllItemsRetrieved));
    }

    /** Adds an observer of item changes. */
    public void addObserver(Observer observer) {
        mObservers.addObserver(observer);
    }

    /** Removes an observer added via {@link #addObserver}. */
    public void removeObserver(Observer observer) {
        mObservers.removeObserver(observer);
    }

    /** Returns a live, read-only view of the active (in progress, pending or paused) items. */
    public Collection<OfflineItem> getActiveItems() {
        return mActiveItemsView;
    }

    /**
     * Returns up to {@link #MAX_RECENT_COMPLETED_ITEMS} completed items started within the last
     * {@link #RECENT_COMPLETED_WINDOW_MS}, most recently started first, including ones that
     * completed before this model was created.
     */
    public List<OfflineItem> getRecentCompletedItems() {
        // Items that have aged past the window are never read again, so drop them here rather than
        // hold them for the rest of the session.
        long cutoffMs = recentCutoffMs();
        mCompletedItems.values().removeIf(item -> item.creationTimeMs < cutoffMs);
        List<OfflineItem> recent = new ArrayList<>(mCompletedItems.values());
        recent.sort((a, b) -> Long.compare(b.creationTimeMs, a.creationTimeMs));
        if (recent.size() > MAX_RECENT_COMPLETED_ITEMS) {
            recent.subList(MAX_RECENT_COMPLETED_ITEMS, recent.size()).clear();
        }
        return recent;
    }

    /** Stops observing the provider and drops all cached items. */
    @Override
    public void destroy() {
        mCallbackController.destroy();
        mProvider.removeObserver(this);
        mActiveItems.clear();
        mCompletedItems.clear();
        mObservers.clear();
    }

    /** Returns whether {@code item} is in progress, pending or paused. */
    public static boolean isActive(OfflineItem item) {
        switch (item.state) {
            case OfflineItemState.IN_PROGRESS:
            case OfflineItemState.PENDING:
            case OfflineItemState.PAUSED:
                return true;
            default:
                return false;
        }
    }

    // OfflineContentProvider.Observer implementation:

    @Override
    public void onItemsAdded(List<OfflineItem> items) {
        for (OfflineItem item : items) {
            recordUpdateBeforeSnapshot(item.id);
            onItemChanged(item);
        }
    }

    @Override
    public void onItemUpdated(OfflineItem item, @Nullable UpdateDelta updateDelta) {
        recordUpdateBeforeSnapshot(item.id);
        onItemChanged(item);
    }

    @Override
    public void onItemRemoved(ContentId id) {
        recordUpdateBeforeSnapshot(id);
        mCompletedItems.remove(id);
        OfflineItem previous = mActiveItems.remove(id);
        for (Observer observer : mObservers) {
            observer.onItemRemoved(id, previous);
        }
    }

    private void recordUpdateBeforeSnapshot(@Nullable ContentId id) {
        if (mIdsUpdatedBeforeSnapshot != null && id != null) {
            mIdsUpdatedBeforeSnapshot.add(id);
        }
    }

    private void onAllItemsRetrieved(List<OfflineItem> items) {
        // Requested once in the constructor, so the set is still present.
        Set<ContentId> updatedBeforeSnapshot = assumeNonNull(mIdsUpdatedBeforeSnapshot);
        mIdsUpdatedBeforeSnapshot = null;
        long recentCutoffMs = recentCutoffMs();

        for (OfflineItem item : items) {
            if (!isTrackable(item)) {
                continue;
            }
            // An observer callback already reported a newer state (e.g. completion or removal);
            // seeding from the stale snapshot would leave the item active forever.
            if (updatedBeforeSnapshot.contains(item.id)) {
                continue;
            }
            if (isActive(item)) {
                // Seeded items are reported like newly added ones, so observers need no separate
                // initialization path.
                onItemChanged(item);
            } else if (item.state == OfflineItemState.COMPLETE
                    && item.creationTimeMs >= recentCutoffMs) {
                // Kept for the tray's recent list only (so older history isn't held in memory),
                // and deliberately not reported: observers treat reported completions as this
                // session's, e.g. for the button's post-completion window, which must not carry
                // over across a restart.
                mCompletedItems.put(assumeNonNull(item.id), item); // Checked by isTrackable().
            }
        }
    }

    private void onItemChanged(OfflineItem item) {
        if (!isTrackable(item)) {
            return;
        }
        ContentId id = assumeNonNull(item.id); // Checked by isTrackable().
        if (item.state == OfflineItemState.COMPLETE) {
            mCompletedItems.put(id, item);
        } else {
            // E.g. a completed item being re-downloaded is no longer a completion.
            mCompletedItems.remove(id);
        }
        OfflineItem previous =
                isActive(item) ? mActiveItems.put(id, item) : mActiveItems.remove(id);
        for (Observer observer : mObservers) {
            observer.onItemChanged(item, previous);
        }
    }

    /** Returns the earliest start time an item may have to count as recent. */
    private static long recentCutoffMs() {
        return TimeUtils.currentTimeMillis() - RECENT_COMPLETED_WINDOW_MS;
    }

    private static boolean isTrackable(OfflineItem item) {
        return item.id != null && !item.isTransient && !item.isSuggested;
    }
}
