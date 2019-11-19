// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.glue;

import org.chromium.base.Callback;
import org.chromium.base.ObserverList;
import org.chromium.chrome.browser.download.home.DownloadManagerUiConfig;
import org.chromium.chrome.browser.widget.ThumbnailProvider;
import org.chromium.components.offline_items_collection.ContentId;
import org.chromium.components.offline_items_collection.LaunchLocation;
import org.chromium.components.offline_items_collection.LegacyHelpers;
import org.chromium.components.offline_items_collection.OfflineContentProvider;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.components.offline_items_collection.ShareCallback;
import org.chromium.components.offline_items_collection.UpdateDelta;
import org.chromium.components.offline_items_collection.VisualsCallback;

import java.util.ArrayList;
import java.util.Collection;
import java.util.List;

/**
 * Glue class responsible for taking an {@link OfflineItem} and determining which backend to hit.
 * This class will eventually be unnecessary once downloads are ported to the generic backend as a
 * {@link OfflineContentProvider}.  However in the short term this layer is needed.
 */
public class OfflineContentProviderGlue implements OfflineContentProvider.Observer {
    private final ObserverList<OfflineContentProvider.Observer> mObservers = new ObserverList<>();
    private final OfflineContentProvider mProvider;
    private final boolean mIncludeOffTheRecord;

    private final boolean mUseNewDownloadPathThumbnails;

    private final DownloadGlue mDownloadProvider;

    private Query mOutstandingQuery;

    /** Creates an {@link OfflineContentProviderGlue} instance. */
    public OfflineContentProviderGlue(
            OfflineContentProvider provider, DownloadManagerUiConfig config) {
        mProvider = provider;
        mIncludeOffTheRecord = config.isOffTheRecord;
        mDownloadProvider = config.useNewDownloadPath ? null : new DownloadGlue(this);
        mUseNewDownloadPathThumbnails = config.useNewDownloadPathThumbnails;

        mProvider.addObserver(this);
    }

    /**
     * Called to tear down the connections to singletons.  This needs to be called when this class
     * is no longer in use.
     */
    public void destroy() {
        if (mDownloadProvider != null) mDownloadProvider.destroy();
        mProvider.removeObserver(this);
    }

    // OfflineContentProvider glue implementation.
    // TODO(dtrainor): Once downloads are behind OfflineContentProvider the interface should be
    // easy use without this layer (we would only pass ID through).
    /** @see OfflineContentProvider#openItem(ContentId) */
    public void openItem(OfflineItem item) {
        if (mDownloadProvider != null && LegacyHelpers.isLegacyDownload(item.id)) {
            mDownloadProvider.openItem(item);
        } else {
            mProvider.openItem(LaunchLocation.DOWNLOAD_HOME, item.id);
        }
    }

    /** @see OfflineContentProvider#removeItem(ContentId) */
    public void removeItem(OfflineItem item) {
        if (mDownloadProvider != null && LegacyHelpers.isLegacyDownload(item.id)) {
            mDownloadProvider.removeItem(item);
        } else {
            mProvider.removeItem(item.id);
        }
    }

    /** @see OfflineContentProvider#cancelDownload(ContentId) */
    public void cancelDownload(OfflineItem item) {
        if (mDownloadProvider != null && LegacyHelpers.isLegacyDownload(item.id)) {
            mDownloadProvider.cancelDownload(item);
        } else {
            mProvider.cancelDownload(item.id);
        }
    }

    /** @see OfflineContentProvider#pauseDownload(ContentId) */
    public void pauseDownload(OfflineItem item) {
        if (mDownloadProvider != null && LegacyHelpers.isLegacyDownload(item.id)) {
            mDownloadProvider.pauseDownload(item);
        } else {
            mProvider.pauseDownload(item.id);
        }
    }

    /** @see OfflineContentProvider#resumeDownload(ContentId) */
    public void resumeDownload(OfflineItem item, boolean hasUserGesture) {
        if (mDownloadProvider != null && LegacyHelpers.isLegacyDownload(item.id)) {
            mDownloadProvider.resumeDownload(item, hasUserGesture);
        } else {
            mProvider.resumeDownload(item.id, hasUserGesture);
        }
    }

    /** @see OfflineContentProvider#getItemById(ContentId, Callback) */
    public void getItemById(ContentId id, Callback<OfflineItem> callback) {
        if (mDownloadProvider != null && LegacyHelpers.isLegacyDownload(id)) {
            mDownloadProvider.getItemById(id, callback);
        } else {
            mProvider.getItemById(id, callback);
        }
    }

    /** @see OfflineContentProvider#getAllItems(Callback) */
    public void getAllItems(Callback<ArrayList<OfflineItem>> callback) {
        if (mOutstandingQuery == null) mOutstandingQuery = new Query();
        mOutstandingQuery.add(callback);
    }

    /**
     * @return Whether or not querying for a thumbail for this {@code id} is supported.
     * @see OfflineContentProvider#getVisualsForItem(ContentId, VisualsCallback)
     */
    public boolean getVisualsForItem(ContentId id, VisualsCallback callback) {
        if (!mUseNewDownloadPathThumbnails && LegacyHelpers.isLegacyDownload(id)) return false;
        mProvider.getVisualsForItem(id, callback);
        return true;
    }

    /**
     * Helper method to remove {@link OfflineItemVisuals} for a {@code id}.  Note that this might
     * not be necessary if everything is using an {@link OfflineContentProvider}.  However the glue
     * layer needs to determine what to do with downloads that have externally managed thumbnails.
     */
    public void removeVisualsForItem(ThumbnailProvider provider, ContentId id) {
        if (mUseNewDownloadPathThumbnails || !LegacyHelpers.isLegacyDownload(id)) return;
        provider.removeThumbnailsFromDisk(id.id);
    }

    /** @see OfflineContentProvider#getShareInfoForItem(ContentId, ShareCallback) */
    public void getShareInfoForItem(OfflineItem item, ShareCallback callback) {
        if (mDownloadProvider != null && LegacyHelpers.isLegacyDownload(item.id)) {
            mDownloadProvider.getShareInfoForItem(item, callback);
        } else {
            mProvider.getShareInfoForItem(item.id, callback);
        }
    }

    /** @see OfflineContentProvider#renameItem(ContentId, String, Callback) */
    public void renameItem(
            OfflineItem item, String targetName, Callback</*RenameResult*/ Integer> callback) {
        if (mDownloadProvider != null && LegacyHelpers.isLegacyDownload(item.id)) {
            mDownloadProvider.renameItem(item, targetName, callback);
        } else {
            mProvider.renameItem(item.id, targetName, callback);
        }
    }

    /** @see OfflineContentProvider#addObserver(OfflineContentProvider.Observer) */
    public void addObserver(OfflineContentProvider.Observer observer) {
        mObservers.addObserver(observer);
    }

    /** @see OfflineContentProvider#removeObserver(OfflineContentProvider.Observer) */
    public void removeObserver(OfflineContentProvider.Observer observer) {
        mObservers.removeObserver(observer);
    }

    // OfflineContentProvider.Observer implementation.
    @Override
    public void onItemsAdded(ArrayList<OfflineItem> items) {
        for (OfflineContentProvider.Observer observer : mObservers) observer.onItemsAdded(items);
    }

    @Override
    public void onItemRemoved(ContentId id) {
        for (OfflineContentProvider.Observer observer : mObservers) observer.onItemRemoved(id);
    }

    @Override
    public void onItemUpdated(OfflineItem item, UpdateDelta updateDelta) {
        for (OfflineContentProvider.Observer observer : mObservers) {
            observer.onItemUpdated(item, updateDelta);
        }
    }

    /**
     * Helper class to request {@link OfflineItem}s from a variety of sources.  This helper will
     * wait until all sources have responded and then notify callbacks of the results.  If
     * subsequent queries come in this class will aggregate them all without making redundant
     * requests to the backends.
     */
    private class Query {
        private final List < Callback < ArrayList<OfflineItem>>> mCallbacks = new ArrayList<>();
        private final ArrayList<OfflineItem> mItems = new ArrayList<OfflineItem>();

        private boolean mDownloadProviderResponded;
        private boolean mDownloadProviderOffTheRecordResponded;
        private boolean mProviderResponded;

        /** Creates a {@link Query} instance. */
        public Query() {
            mDownloadProviderOffTheRecordResponded = !mIncludeOffTheRecord;

            if (mDownloadProvider == null) {
                mDownloadProviderResponded = true;
                mDownloadProviderOffTheRecordResponded = true;
            } else {
                if (mIncludeOffTheRecord) {
                    mDownloadProvider.getAllItems(
                            items -> addOffTheRecordDownloads(items), true /* offTheRecord */);
                }
                mDownloadProvider.getAllItems(
                        items -> addDownloads(items), false /* offTheRecord */);
            }

            mProvider.getAllItems(items -> addOfflineItems(items));
        }

        /** Adds {@code callback} to be notified when the backends all respond with items. */
        public void add(Callback<ArrayList<OfflineItem>> callback) {
            mCallbacks.add(callback);
        }

        private void addDownloads(Collection<OfflineItem> items) {
            mItems.addAll(items);
            mDownloadProviderResponded = true;

            checkDispatch();
        }

        private void addOffTheRecordDownloads(Collection<OfflineItem> items) {
            mItems.addAll(items);
            mDownloadProviderOffTheRecordResponded = true;

            checkDispatch();
        }

        private void addOfflineItems(Collection<OfflineItem> items) {
            mItems.addAll(items);
            mProviderResponded = true;

            checkDispatch();
        }

        private void checkDispatch() {
            if (!mProviderResponded || !mDownloadProviderResponded
                    || !mDownloadProviderOffTheRecordResponded) {
                return;
            }

            // Clear out this reference before notifying the callbacks in case they are reentrant.
            mOutstandingQuery = null;

            for (Callback<ArrayList<OfflineItem>> callback : mCallbacks) callback.onResult(mItems);
        }
    }
}
