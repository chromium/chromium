// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.glue;

import android.text.TextUtils;

import org.chromium.base.Callback;
import org.chromium.base.CollectionUtil;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.browser.download.DownloadInfo;
import org.chromium.chrome.browser.download.DownloadItem;
import org.chromium.chrome.browser.download.DownloadManagerService;
import org.chromium.chrome.browser.download.DownloadManagerService.DownloadObserver;
import org.chromium.chrome.browser.download.DownloadOpenSource;
import org.chromium.chrome.browser.download.DownloadUtils;
import org.chromium.components.offline_items_collection.ContentId;
import org.chromium.components.offline_items_collection.LegacyHelpers;
import org.chromium.components.offline_items_collection.OfflineContentProvider;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.components.offline_items_collection.OfflineItemShareInfo;
import org.chromium.components.offline_items_collection.ShareCallback;
import org.chromium.components.offline_items_collection.VisualsCallback;
import org.chromium.content_public.browser.UiThreadTaskTraits;

import java.util.ArrayList;
import java.util.List;

/**
 * Glue class responsible for converting downloads from {@link DownloadManagerService} into some
 * semblance of {@link OfflineContentProvider}.  This class handles (1) all download idiosyncrasies
 * that need to happen in Java before hitting the service and (2) converting from
 * {@link DownloadItem}s to {@link OfflineItem}s.
 */
public class DownloadGlue implements DownloadObserver {
    private final ArrayList < Callback < ArrayList<OfflineItem>>> mRequests = new ArrayList<>();
    private final ArrayList < Callback < ArrayList<OfflineItem>>> mOffTheRecordRequests =
            new ArrayList<>();

    private final OfflineContentProvider.Observer mDelegate;

    /** Creates an instance of a {@link DownloadGlue}. */
    public DownloadGlue(OfflineContentProvider.Observer delegate) {
        mDelegate = delegate;
        DownloadManagerService.getDownloadManagerService().addDownloadObserver(this);
    }

    /**
     * Called to tear down the connections to singletons.  This needs to be called when this class
     * is no longer in use.
     */
    public void destroy() {
        DownloadManagerService.getDownloadManagerService().removeDownloadObserver(this);
    }

    // DownloadObserver (OfflineContentProvider.Observer glue) implementation.
    /** @see OfflineContentProvider.Observer#onItemsAdded(ArrayList) */
    @Override
    public void onDownloadItemCreated(DownloadItem item) {
        if (!canShowDownloadItem(item)) return;
        mDelegate.onItemsAdded(CollectionUtil.newArrayList(DownloadItem.createOfflineItem(item)));
    }

    /** @see OfflineContentProvider.Observer#onItemUpdated(OfflineItem) */
    @Override
    public void onDownloadItemUpdated(DownloadItem item) {
        if (!canShowDownloadItem(item)) return;

        OfflineItem offlineItem = DownloadItem.createOfflineItem(item);
        mDelegate.onItemUpdated(offlineItem, null);

        if (offlineItem.externallyRemoved) {
            PostTask.postTask(UiThreadTaskTraits.DEFAULT, () -> removeItem(offlineItem));
        }
    }

    /** @see OfflineContentProvider.Observer#onItemRemoved(ContentId) */
    @Override
    public void onDownloadItemRemoved(String guid, boolean isOffTheRecord) {
        mDelegate.onItemRemoved(LegacyHelpers.buildLegacyContentId(false, guid));
    }

    @Override
    public void onAllDownloadsRetrieved(List<DownloadItem> items, boolean offTheRecord) {
        List < Callback < ArrayList<OfflineItem>>> list =
                offTheRecord ? mOffTheRecordRequests : mRequests;
        if (list.isEmpty()) return;

        ArrayList<OfflineItem> offlineItems = new ArrayList<>();
        for (DownloadItem item : items) {
            if (!canShowDownloadItem(item)) continue;
            offlineItems.add(DownloadItem.createOfflineItem(item));
        }

        // Copy the list and clear the original in case the callbacks are reentrant.
        List < Callback < ArrayList<OfflineItem>>> listCopy = new ArrayList<>(list);
        list.clear();

        for (Callback<ArrayList<OfflineItem>> callback : listCopy) callback.onResult(offlineItems);
    }

    @Override
    public void onAddOrReplaceDownloadSharedPreferenceEntry(ContentId id) {}

    // OfflineContentProvider glue implementation.
    /** @see OfflineContentProvider#openItem(ContentId) */
    public void openItem(OfflineItem item) {
        // TODO(shaktisahu): May be pass metrics as a param.
        DownloadManagerService.getDownloadManagerService().openDownload(
                item.id, item.isOffTheRecord, DownloadOpenSource.DOWNLOAD_HOME);
    }

    /** @see OfflineContentProvider#removeItem(ContentId) */
    public void removeItem(OfflineItem item) {
        DownloadManagerService.getDownloadManagerService().removeDownload(
                item.id.id, item.isOffTheRecord, item.externallyRemoved);
        FileDeletionQueue.get().delete(item.filePath);
    }

    /** @see OfflineContentProvider#cancelDownload(ContentId) */
    public void cancelDownload(OfflineItem item) {
        DownloadManagerService.getDownloadManagerService().cancelDownload(
                item.id, item.isOffTheRecord);
    }

    /** @see OfflineContentProvider#pauseDownload(ContentId) */
    public void pauseDownload(OfflineItem item) {
        DownloadManagerService.getDownloadManagerService().pauseDownload(
                item.id, item.isOffTheRecord);
    }

    /** @see OfflineContentProvider#resumeDownload(ContentId, boolean) */
    public void resumeDownload(OfflineItem item, boolean hasUserGesture) {
        DownloadInfo.Builder builder = DownloadInfo.builderFromOfflineItem(item, null);

        // This is a temporary hack to work around the assumption that the DownloadItem passed to
        // DownloadManagerService#resumeDownload() will not be paused.
        builder.setIsPaused(false);

        DownloadItem downloadItem =
                new DownloadItem(false /* useAndroidDownloadManager */, builder.build());

        if (item.isResumable) {
            DownloadManagerService.getDownloadManagerService().resumeDownload(
                    item.id, downloadItem, hasUserGesture);
        } else {
            DownloadManagerService.getDownloadManagerService().retryDownload(
                    item.id, downloadItem, hasUserGesture);
        }
    }

    /** @see OfflineContentProvider#getItemById(ContentId, Callback) */
    public void getItemById(ContentId id, Callback<OfflineItem> callback) {
        assert false : "Not supported.";
        PostTask.postTask(UiThreadTaskTraits.DEFAULT, () -> callback.onResult(null));
    }

    /** @see OfflineContentProvider#getAllItems(Callback) */
    public void getAllItems(Callback<ArrayList<OfflineItem>> callback, boolean offTheRecord) {
        List < Callback < ArrayList<OfflineItem>>> list =
                offTheRecord ? mOffTheRecordRequests : mRequests;

        list.add(callback);
        if (list.size() > 1) return;
        DownloadManagerService.getDownloadManagerService().getAllDownloads(offTheRecord);
    }

    /** @see OfflineContentProvider#getVisualsForItem(ContentId, VisualsCallback) */
    public void getVisualsForItem(ContentId id, VisualsCallback callback) {
        PostTask.postTask(UiThreadTaskTraits.DEFAULT, () -> callback.onVisualsAvailable(id, null));
    }

    /** @see OfflineContentProvider#getShareInfoForItem(ContentId, ShareCallback) */
    public void getShareInfoForItem(OfflineItem item, ShareCallback callback) {
        OfflineItemShareInfo info = new OfflineItemShareInfo();
        info.uri = DownloadUtils.getUriForItem(item.filePath);
        PostTask.postTask(
                UiThreadTaskTraits.DEFAULT, () -> callback.onShareInfoAvailable(item.id, info));
    }

    /** @see OfflineContentProvider#renameItem(ContentId, String, Callback)*/
    public void renameItem(
            OfflineItem item, String name, Callback</*RenameResult*/ Integer> callback) {
        DownloadManagerService.getDownloadManagerService().renameDownload(
                item.id, name, callback, item.isOffTheRecord);
    }

    /**
     * There could be some situations where we can't visually represent this download in the UI.
     * This should be handled in native/be more generic, but it's here in the glue for now.
     * @return Whether or not {@code item} should be shown in the UI.
     */
    private static boolean canShowDownloadItem(DownloadItem item) {
        if (TextUtils.isEmpty(item.getDownloadInfo().getFilePath())) return false;
        if (TextUtils.isEmpty(item.getDownloadInfo().getFileName())) return false;
        return true;
    }
}
