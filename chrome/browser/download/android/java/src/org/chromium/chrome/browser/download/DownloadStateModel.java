// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import static org.chromium.build.NullUtil.assumeNonNull;

import org.chromium.base.CallbackController;
import org.chromium.base.ObserverList;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.offline_items_collection.ContentId;
import org.chromium.components.offline_items_collection.OfflineContentProvider;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.components.offline_items_collection.OfflineItemState;
import org.chromium.components.offline_items_collection.UpdateDelta;

import java.util.Collection;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * Observes an {@link OfflineContentProvider} once and caches the download items that the download
 * toolbar UI cares about, so several consumers can share one observer and one item cache. The
 * Android analogue of desktop's {@code DownloadBubbleUpdateService}.
 *
 * <p>Only trackable items are reported: transient items (e.g. Background Fetch jobs, which only
 * surface as notifications) and suggested items (e.g. prefetched content) are ignored, as in {@code
 * DownloadMessageUiControllerImpl}. Items from all profiles are tracked.
 */
@NullMarked
public class DownloadStateModel implements OfflineContentProvider.Observer, Destroyable {
    /** Notified synchronously after the cache has been updated. */
    public interface Observer {
        /**
         * A trackable item was added or updated. Items already active when the initial {@link
         * OfflineContentProvider#getAllItems} snapshot arrives are reported here too, with a null
         * {@code previous}; observers added afterwards should read {@link
         * DownloadStateModel#getActiveItems()}.
         *
         * @param item The latest data for the item.
         * @param previous The item's cached data before this change, or null if it was not cached.
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

    /** Stops observing the provider and drops all cached items. */
    @Override
    public void destroy() {
        mCallbackController.destroy();
        mProvider.removeObserver(this);
        mActiveItems.clear();
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

        // Only items still in flight are seeded; already-ended items are ignored so the
        // post-completion window does not carry over across a restart. Seeded items are reported
        // like newly added ones, so observers need no separate initialization path.
        for (OfflineItem item : items) {
            if (!isTrackable(item) || !isActive(item)) {
                continue;
            }
            // An observer callback already reported a newer state (e.g. completion or removal);
            // seeding from the stale snapshot would leave the item active forever.
            if (updatedBeforeSnapshot.contains(item.id)) {
                continue;
            }
            onItemChanged(item);
        }
    }

    private void onItemChanged(OfflineItem item) {
        if (!isTrackable(item)) {
            return;
        }
        ContentId id = assumeNonNull(item.id); // Checked by isTrackable().
        OfflineItem previous =
                isActive(item) ? mActiveItems.put(id, item) : mActiveItems.remove(id);
        for (Observer observer : mObservers) {
            observer.onItemChanged(item, previous);
        }
    }

    private static boolean isTrackable(OfflineItem item) {
        return item.id != null && !item.isTransient && !item.isSuggested;
    }
}
