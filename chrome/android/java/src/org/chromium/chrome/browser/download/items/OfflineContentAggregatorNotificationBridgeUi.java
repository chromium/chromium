// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.items;

import org.chromium.chrome.browser.download.DownloadInfo;
import org.chromium.chrome.browser.download.DownloadItem;
import org.chromium.chrome.browser.download.DownloadNotifier;
import org.chromium.chrome.browser.download.DownloadServiceDelegate;
import org.chromium.chrome.browser.profiles.OTRProfileID;
import org.chromium.components.offline_items_collection.ContentId;
import org.chromium.components.offline_items_collection.LegacyHelpers;
import org.chromium.components.offline_items_collection.OfflineContentProvider;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.components.offline_items_collection.OfflineItemState;
import org.chromium.components.offline_items_collection.OfflineItemVisuals;
import org.chromium.components.offline_items_collection.OpenParams;
import org.chromium.components.offline_items_collection.UpdateDelta;
import org.chromium.components.offline_items_collection.VisualsCallback;
import org.chromium.ui.permissions.ContextualNotificationPermissionRequester;

import java.util.HashMap;
import java.util.List;

/**
 * A glue class that bridges the Profile-attached OfflineContentProvider with the
 * download notification code (SystemDownloadNotifier and DownloadServiceDelegate).
 */
public class OfflineContentAggregatorNotificationBridgeUi
        implements DownloadServiceDelegate, OfflineContentProvider.Observer, VisualsCallback {
    // TODO(dtrainor): Should this just be part of the OfflineContentProvider callback guarantee?
    private static final OfflineItemVisuals sEmptyOfflineItemVisuals = new OfflineItemVisuals();

    private final OfflineContentProvider mProvider;

    private final DownloadNotifier mUi;

    /** Holds a list of {@link OfflineItem} updates that are waiting for visuals. */
    private final HashMap<ContentId, OfflineItem> mOutstandingRequests = new HashMap<>();

    /**
     * Holds a list of {@link OfflineItemVisuals} for all {@link OfflineItem}s that are currently in
     * progress.  Once an {@link OfflineItem} is no longer in progress it will be removed from this
     * cache.
     * TODO(dtrainor): Flush this list aggressively if we get onLowMemory/onTrimMemory.
     * TODO(dtrainor): Add periodic clean up in case something goes wrong with the underlying
     * downloads.
     */
    private final HashMap<ContentId, OfflineItemVisuals> mVisualsCache = new HashMap<>();

    /** Creates a new OfflineContentAggregatorNotificationBridgeUi based on {@code provider}. */
    public OfflineContentAggregatorNotificationBridgeUi(
            OfflineContentProvider provider, DownloadNotifier notifier) {
        mProvider = provider;
        mUi = notifier;

        mProvider.addObserver(this);
    }

    /** Destroys this class and detaches it from associated objects. */
    public void destroy() {
        mProvider.removeObserver(this);
        destroyServiceDelegate();
    }

    /** @see OfflineContentProvider#openItem(OpenParams, ContentId) */
    public void openItem(OpenParams openParams, ContentId id) {
        mProvider.openItem(openParams, id);
    }

    // OfflineContentProvider.Observer implementation.
    @Override
    public void onItemsAdded(List<OfflineItem> items) {
        for (int i = 0; i < items.size(); ++i) getVisualsAndUpdateItem(items.get(i), null);
    }

    @Override
    public void onItemRemoved(ContentId id) {
        mOutstandingRequests.remove(id);
        mVisualsCache.remove(id);
        mUi.notifyDownloadCanceled(id);
    }

    @Override
    public void onItemUpdated(OfflineItem item, UpdateDelta updateDelta) {
        getVisualsAndUpdateItem(item, updateDelta);
    }

    // OfflineContentProvider.VisualsCallback implementation.
    @Override
    public void onVisualsAvailable(ContentId id, OfflineItemVisuals visuals) {
        OfflineItem item = mOutstandingRequests.remove(id);
        if (item == null) return;

        if (visuals == null) visuals = sEmptyOfflineItemVisuals;

        // Only cache the visuals if the update we are about to push is interesting and we think we
        // will need them in the future.
        if (shouldCacheVisuals(item)) mVisualsCache.put(id, visuals);
        pushItemToUi(item, visuals);
    }

    // DownloadServiceDelegate implementation.
    @Override
    public void cancelDownload(ContentId id, OTRProfileID otrProfileID) {
        mProvider.cancelDownload(id);
    }

    @Override
    public void pauseDownload(ContentId id, OTRProfileID otrProfileID) {
        mProvider.pauseDownload(id);
    }

    @Override
    public void resumeDownload(ContentId id, DownloadItem item) {
        mProvider.resumeDownload(id);
    }

    @Override
    public void destroyServiceDelegate() {}

    private void getVisualsAndUpdateItem(OfflineItem item, UpdateDelta updateDelta) {
        if (shouldIgnoreUpdate(item, updateDelta)) return;
        if (updateDelta != null && updateDelta.visualsChanged) mVisualsCache.remove(item.id);
        if (needsVisualsForUi(item)) {
            if (!mVisualsCache.containsKey(item.id)) {
                // We don't have any visuals for this item yet.  Stash the current OfflineItem and,
                // if we haven't already, queue up a request for the visuals.
                // TODO(dtrainor): Check if this delay is too much.  If so, just send the update
                // through and we can push a new notification when the visuals arrive.
                boolean requestVisuals = !mOutstandingRequests.containsKey(item.id);
                mOutstandingRequests.put(item.id, item);
                if (requestVisuals) mProvider.getVisualsForItem(item.id, this);
                return;
            }
        } else {
            // We don't need the visuals to show this item at this point.  Cancel any requests.
            mOutstandingRequests.remove(item.id);
            mVisualsCache.remove(item.id);
        }

        pushItemToUi(item, mVisualsCache.get(item.id));
        // We will no longer be needing the visuals for this item after this notification.
        if (!shouldCacheVisuals(item)) mVisualsCache.remove(item.id);
    }

    private void pushItemToUi(OfflineItem item, OfflineItemVisuals visuals) {
        // TODO(http://crbug.com/855141): Find a cleaner way to hide unimportant UI updates.
        // If it's a suggested page, or the user choose to download later. Do not add it to the
        // notification UI.
        if (item.isSuggested) return;

        // If the download is cancelled, no need to DownloadInfo object and it is enough to notify
        // that the download is canceled.
        if (item.state == OfflineItemState.CANCELLED) {
            mUi.notifyDownloadCanceled(item.id);
            return;
        }

        // Request notification permission if needed.
        ContextualNotificationPermissionRequester.getInstance().requestPermissionIfNeeded();

        DownloadInfo info = DownloadInfo.fromOfflineItem(item, visuals);
        switch (item.state) {
            case OfflineItemState.IN_PROGRESS:
                mUi.notifyDownloadProgress(info, item.creationTimeMs, item.allowMetered);
                break;
            case OfflineItemState.COMPLETE:
                mUi.notifyDownloadSuccessful(info, -1L, false, item.isOpenable);
                break;
            case OfflineItemState.INTERRUPTED:
                mUi.notifyDownloadInterrupted(
                        info, !LegacyHelpers.isLegacyDownload(item.id), item.pendingState);
                break;
            case OfflineItemState.PAUSED:
                mUi.notifyDownloadPaused(info);
                break;
            case OfflineItemState.FAILED:
                mUi.notifyDownloadFailed(info);
                break;
            case OfflineItemState.PENDING:
                mUi.notifyDownloadPaused(info);
                break;
            default:
                assert false : "Unexpected OfflineItem state.";
        }
    }

    private boolean needsVisualsForUi(OfflineItem item) {
        if (item.ignoreVisuals) return false;
        switch (item.state) {
            case OfflineItemState.IN_PROGRESS:
            case OfflineItemState.PENDING:
            case OfflineItemState.COMPLETE:
            case OfflineItemState.INTERRUPTED:
            case OfflineItemState.FAILED:
            case OfflineItemState.PAUSED:
                return true;
                // OfflineItemState.CANCELLED
            default:
                return false;
        }
    }

    private boolean shouldCacheVisuals(OfflineItem item) {
        if (item.ignoreVisuals) return false;
        switch (item.state) {
            case OfflineItemState.IN_PROGRESS:
            case OfflineItemState.PENDING:
            case OfflineItemState.INTERRUPTED:
            case OfflineItemState.PAUSED:
            case OfflineItemState.COMPLETE:
                return true;
                // OfflineItemState.FAILED,
                // OfflineItemState.CANCELLED
            default:
                return false;
        }
    }

    private boolean shouldIgnoreUpdate(OfflineItem item, UpdateDelta updateDelta) {
        // We only ignore updates for completed items, if there is no significant state change
        // update.
        if (item.state != OfflineItemState.COMPLETE) return false;
        if (updateDelta == null) return false;
        if (updateDelta.stateChanged || updateDelta.visualsChanged) return false;
        return true;
    }
}
