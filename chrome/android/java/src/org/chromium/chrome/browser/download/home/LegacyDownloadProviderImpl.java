// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home;

import android.text.TextUtils;

import org.chromium.base.Callback;
import org.chromium.base.ObserverList;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.browser.download.DownloadInfo;
import org.chromium.chrome.browser.download.DownloadItem;
import org.chromium.chrome.browser.download.DownloadManagerService;
import org.chromium.chrome.browser.download.DownloadManagerService.DownloadObserver;
import org.chromium.chrome.browser.download.DownloadMetrics;
import org.chromium.chrome.browser.download.DownloadOpenSource;
import org.chromium.chrome.browser.download.DownloadUtils;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.profiles.OTRProfileID;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileKey;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.components.offline_items_collection.ContentId;
import org.chromium.components.offline_items_collection.LegacyHelpers;
import org.chromium.components.offline_items_collection.OfflineContentProvider;
import org.chromium.components.offline_items_collection.OfflineContentProvider.Observer;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.components.offline_items_collection.OfflineItemSchedule;
import org.chromium.components.offline_items_collection.OfflineItemShareInfo;
import org.chromium.components.offline_items_collection.ShareCallback;
import org.chromium.components.offline_items_collection.VisualsCallback;
import org.chromium.content_public.browser.UiThreadTaskTraits;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;

/**
 * Glue class responsible for converting downloads from {@link DownloadManagerService} into some
 * semblance of {@link OfflineContentProvider}.  This class handles (1) all download idiosyncrasies
 * that need to happen in Java before hitting the service and (2) converting from
 * {@link DownloadItem}s to {@link OfflineItem}s.
 */
class LegacyDownloadProviderImpl
        implements DownloadObserver, LegacyDownloadProvider, ProfileManager.Observer {
    private HashMap<ProfileKey, ArrayList<Callback<ArrayList<OfflineItem>>>> mRequestsMap =
            new HashMap<>();

    private final ObserverList<OfflineContentProvider.Observer> mObservers = new ObserverList<>();

    /** Creates an instance of a {@link LegacyDownloadProvider}. */
    public LegacyDownloadProviderImpl() {
        DownloadManagerService.getDownloadManagerService().addDownloadObserver(this);
    }

    // DownloadObserver (OfflineContentProvider.Observer glue) implementation.
    /** @see OfflineContentProvider.Observer#onItemsAdded(ArrayList) */
    @Override
    public void onDownloadItemCreated(DownloadItem item) {
        if (!canShowDownloadItem(item)) return;
        for (OfflineContentProvider.Observer observer : mObservers) {
            observer.onItemsAdded(Collections.singletonList(DownloadItem.createOfflineItem(item)));
        }
    }

    /** @see OfflineContentProvider.Observer#onItemUpdated(OfflineItem) */
    @Override
    public void onDownloadItemUpdated(DownloadItem item) {
        if (!canShowDownloadItem(item)) return;

        OfflineItem offlineItem = DownloadItem.createOfflineItem(item);
        for (OfflineContentProvider.Observer observer : mObservers) {
            observer.onItemUpdated(offlineItem, null);
        }

        if (offlineItem.externallyRemoved) {
            PostTask.postTask(UiThreadTaskTraits.DEFAULT, () -> removeItem(offlineItem));
        }
    }

    /** @see OfflineContentProvider.Observer#onItemRemoved(ContentId) */
    @Override
    public void onDownloadItemRemoved(String guid) {
        for (OfflineContentProvider.Observer observer : mObservers) {
            observer.onItemRemoved(LegacyHelpers.buildLegacyContentId(false, guid));
        }
    }

    @Override
    public void onAllDownloadsRetrieved(List<DownloadItem> items, ProfileKey profileKey) {
        List<Callback<ArrayList<OfflineItem>>> list = getRequestList(profileKey);
        if (list.isEmpty()) return;

        ArrayList<OfflineItem> offlineItems = new ArrayList<>();
        for (DownloadItem item : items) {
            if (!canShowDownloadItem(item)) continue;
            offlineItems.add(DownloadItem.createOfflineItem(item));
        }

        // Copy the list and clear the original in case the callbacks are reentrant.
        List<Callback<ArrayList<OfflineItem>>> listCopy = new ArrayList<>(list);
        list.clear();
        mRequestsMap.remove(profileKey);

        for (Callback<ArrayList<OfflineItem>> callback : listCopy) callback.onResult(offlineItems);
    }

    @Override
    public void onAddOrReplaceDownloadSharedPreferenceEntry(ContentId id) {}

    // LegacyDownloadProvider implementation.
    @Override
    public void addObserver(Observer observer) {
        mObservers.addObserver(observer);
    }

    @Override
    public void removeObserver(Observer observer) {
        mObservers.removeObserver(observer);
    }

    @Override
    public void destroy() {
        DownloadManagerService.getDownloadManagerService().removeDownloadObserver(this);
    }

    @Override
    public void openItem(OfflineItem item) {
        // TODO(shaktisahu): May be pass metrics as a param.
        DownloadManagerService.getDownloadManagerService().openDownload(item.id,
                OTRProfileID.deserialize(item.otrProfileId), DownloadOpenSource.DOWNLOAD_HOME);
    }

    @Override
    public void removeItem(OfflineItem item) {
        DownloadManagerService.getDownloadManagerService().removeDownload(
                item.id.id, OTRProfileID.deserialize(item.otrProfileId), item.externallyRemoved);
        FileDeletionQueue.get().delete(item.filePath);
    }

    @Override
    public void cancelDownload(OfflineItem item) {
        DownloadMetrics.recordDownloadCancel(DownloadMetrics.CancelFrom.CANCEL_DOWNLOAD_HOME);
        DownloadManagerService.getDownloadManagerService().cancelDownload(
                item.id, OTRProfileID.deserialize(item.otrProfileId));
    }

    @Override
    public void pauseDownload(OfflineItem item) {
        DownloadManagerService.getDownloadManagerService().pauseDownload(
                item.id, OTRProfileID.deserialize(item.otrProfileId));
    }

    @Override
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

    @Override
    public void getItemById(ContentId id, Callback<OfflineItem> callback) {
        assert false : "Not supported.";
        PostTask.postTask(UiThreadTaskTraits.DEFAULT, callback.bind(null));
    }

    @Override
    public void getAllItems(Callback<ArrayList<OfflineItem>> callback, OTRProfileID otrProfileID) {
        ProfileKey profileKey = IncognitoUtils.getProfileKeyFromOTRProfileID(otrProfileID);
        ArrayList<Callback<ArrayList<OfflineItem>>> list = getRequestList(profileKey);

        list.add(callback);
        mRequestsMap.put(profileKey, list);
        if (list.size() > 1) return;
        DownloadManagerService.getDownloadManagerService().getAllDownloads(otrProfileID);
    }

    @Override
    public void getVisualsForItem(ContentId id, VisualsCallback callback) {
        PostTask.postTask(UiThreadTaskTraits.DEFAULT, () -> callback.onVisualsAvailable(id, null));
    }

    @Override
    public void getShareInfoForItem(OfflineItem item, ShareCallback callback) {
        OfflineItemShareInfo info = new OfflineItemShareInfo();
        info.uri = DownloadUtils.getUriForItem(item.filePath);
        PostTask.postTask(
                UiThreadTaskTraits.DEFAULT, () -> callback.onShareInfoAvailable(item.id, info));
    }

    @Override
    public void renameItem(
            OfflineItem item, String name, Callback</*RenameResult*/ Integer> callback) {
        DownloadManagerService.getDownloadManagerService().renameDownload(
                item.id, name, callback, OTRProfileID.deserialize(item.otrProfileId));
    }

    @Override
    public void changeSchedule(final OfflineItem item, final OfflineItemSchedule schedule) {
        DownloadManagerService.getDownloadManagerService().changeSchedule(
                item.id, schedule, OTRProfileID.deserialize(item.otrProfileId));
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

    private ArrayList getRequestList(ProfileKey profileKey) {
        return mRequestsMap.get(profileKey) == null ? new ArrayList<>()
                                                    : mRequestsMap.get(profileKey);
    }

    @Override
    public void onProfileAdded(Profile profile) {}

    @Override
    public void onProfileDestroyed(Profile profile) {
        mRequestsMap.remove(profile.getProfileKey());
    }
}
