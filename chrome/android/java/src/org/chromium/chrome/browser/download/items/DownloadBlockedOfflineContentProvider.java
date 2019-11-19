// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.items;

import org.chromium.base.Callback;
import org.chromium.base.ObserverList;
import org.chromium.components.offline_items_collection.ContentId;
import org.chromium.components.offline_items_collection.LaunchLocation;
import org.chromium.components.offline_items_collection.LegacyHelpers;
import org.chromium.components.offline_items_collection.OfflineContentProvider;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.components.offline_items_collection.ShareCallback;
import org.chromium.components.offline_items_collection.UpdateDelta;
import org.chromium.components.offline_items_collection.VisualsCallback;

import java.util.ArrayList;

/**
 * Filters out download offline items till downloads backend fully supports offline content
 * provider.
 * TODO(crbug.com/857543): Remove this after downloads is migrated to use OfflineContentProvider.
 */
class DownloadBlockedOfflineContentProvider
        implements OfflineContentProvider, OfflineContentProvider.Observer {
    private OfflineContentProvider mProvider;
    private ObserverList<Observer> mObservers;

    public DownloadBlockedOfflineContentProvider(OfflineContentProvider provider) {
        mProvider = provider;
        mObservers = new ObserverList<>();
        mProvider.addObserver(this);
    }

    @Override
    public void openItem(@LaunchLocation int location, ContentId id) {
        assert !LegacyHelpers.isLegacyDownload(id);
        mProvider.openItem(location, id);
    }

    @Override
    public void removeItem(ContentId id) {
        assert !LegacyHelpers.isLegacyDownload(id);
        mProvider.removeItem(id);
    }

    @Override
    public void cancelDownload(ContentId id) {
        assert !LegacyHelpers.isLegacyDownload(id);
        mProvider.cancelDownload(id);
    }

    @Override
    public void pauseDownload(ContentId id) {
        assert !LegacyHelpers.isLegacyDownload(id);
        mProvider.pauseDownload(id);
    }

    @Override
    public void resumeDownload(ContentId id, boolean hasUserGesture) {
        assert !LegacyHelpers.isLegacyDownload(id);
        mProvider.resumeDownload(id, hasUserGesture);
    }

    @Override
    public void getItemById(ContentId id, Callback<OfflineItem> callback) {
        assert !LegacyHelpers.isLegacyDownload(id);
        mProvider.getItemById(id, callback);
    }

    @Override
    public void getAllItems(Callback<ArrayList<OfflineItem>> callback) {
        mProvider.getAllItems(new Callback<ArrayList<OfflineItem>>() {
            @Override
            public void onResult(ArrayList<OfflineItem> items) {
                callback.onResult(getFilteredList(items));
            }
        });
    }

    @Override
    public void getVisualsForItem(ContentId id, VisualsCallback callback) {
        assert !LegacyHelpers.isLegacyDownload(id);
        mProvider.getVisualsForItem(id, callback);
    }

    @Override
    public void renameItem(ContentId id, String name, Callback<Integer> callback) {
        assert !LegacyHelpers.isLegacyDownload(id);
        mProvider.renameItem(id, name, callback);
    }

    @Override
    public void addObserver(Observer observer) {
        mObservers.addObserver(observer);
    }

    @Override
    public void removeObserver(Observer observer) {
        mObservers.removeObserver(observer);
    }

    @Override
    public void onItemsAdded(ArrayList<OfflineItem> items) {
        ArrayList<OfflineItem> filteredList = getFilteredList(items);
        for (Observer observer : mObservers) {
            observer.onItemsAdded(filteredList);
        }
    }

    @Override
    public void onItemRemoved(ContentId id) {
        if (LegacyHelpers.isLegacyDownload(id)) return;
        for (Observer observer : mObservers) {
            observer.onItemRemoved(id);
        }
    }

    @Override
    public void onItemUpdated(OfflineItem item, UpdateDelta updateDelta) {
        if (LegacyHelpers.isLegacyDownload(item.id)) return;
        for (Observer observer : mObservers) {
            observer.onItemUpdated(item, updateDelta);
        }
    }

    private ArrayList<OfflineItem> getFilteredList(ArrayList<OfflineItem> items) {
        ArrayList<OfflineItem> filteredList = new ArrayList<>();
        for (OfflineItem item : items) {
            if (LegacyHelpers.isLegacyDownload(item.id)) continue;
            filteredList.add(item);
        }
        return filteredList;
    }

    @Override
    public void getShareInfoForItem(ContentId id, ShareCallback callback) {
        assert !LegacyHelpers.isLegacyDownload(id);
        mProvider.getShareInfoForItem(id, callback);
    }
}
