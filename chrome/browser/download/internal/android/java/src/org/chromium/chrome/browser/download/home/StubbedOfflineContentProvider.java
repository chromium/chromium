// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;

import android.os.Handler;
import android.os.Looper;

import org.chromium.base.Callback;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.components.offline_items_collection.ContentId;
import org.chromium.components.offline_items_collection.OfflineContentProvider;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.components.offline_items_collection.OpenParams;
import org.chromium.components.offline_items_collection.RenameResult;
import org.chromium.components.offline_items_collection.ShareCallback;
import org.chromium.components.offline_items_collection.VisualsCallback;

import java.util.ArrayList;

/** Stubs out the OfflineContentProvider. */
public class StubbedOfflineContentProvider implements OfflineContentProvider {
    private final Handler mHandler;
    private final CallbackHelper mDeleteItemCallback;
    private final ArrayList<OfflineItem> mItems;
    private OfflineContentProvider.Observer mObserver;

    public StubbedOfflineContentProvider() {
        mHandler = new Handler(Looper.getMainLooper());
        mDeleteItemCallback = new CallbackHelper();
        mItems = new ArrayList<>();
    }

    public ArrayList<OfflineItem> getItems() {
        return mItems;
    }

    public void addItem(OfflineItem item) {
        mItems.add(item);

        ArrayList<OfflineItem> list = new ArrayList<>();
        list.add(item);
        if (mObserver != null) mObserver.onItemsAdded(list);
    }

    public void setObserver(OfflineContentProvider.Observer newObserver) {
        mObserver = newObserver;
    }

    @Override
    public void addObserver(OfflineContentProvider.Observer addedObserver) {
        assertNull(mObserver);
        mObserver = addedObserver;
    }

    @Override
    public void removeObserver(OfflineContentProvider.Observer removedObserver) {
        assertEquals(mObserver, removedObserver);
        mObserver = null;
    }

    @Override
    public void getItemById(ContentId id, Callback<OfflineItem> callback) {
        mHandler.post(callback.bind(null));
    }

    @Override
    public void getAllItems(Callback<ArrayList<OfflineItem>> callback) {
        mHandler.post(callback.bind(mItems));
    }

    @Override
    public void getVisualsForItem(ContentId id, VisualsCallback callback) {
        mHandler.post(() -> callback.onVisualsAvailable(id, null));
    }

    @Override
    public void getShareInfoForItem(ContentId id, ShareCallback callback) {
        mHandler.post(() -> callback.onShareInfoAvailable(id, null));
    }

    @Override
    public void removeItem(ContentId id) {
        for (OfflineItem item : mItems) {
            if (item.id.equals(id)) {
                mItems.remove(item);
                break;
            }
        }

        mHandler.post(
                () -> {
                    if (mObserver != null) mObserver.onItemRemoved(id);
                    mDeleteItemCallback.notifyCalled();
                });
    }

    @Override
    public void openItem(OpenParams openParams, ContentId id) {}

    @Override
    public void pauseDownload(ContentId id) {}

    @Override
    public void resumeDownload(ContentId id) {}

    @Override
    public void cancelDownload(ContentId id) {}

    @Override
    public void renameItem(ContentId id, String name, Callback<Integer /*RenameResult*/> callback) {
        mHandler.post(callback.bind(RenameResult.SUCCESS));
    }

    /** Triggers the onItemUpdated method of any observer. */
    protected void notifyObservers(ContentId id) {
        if (mObserver != null) mObserver.onItemUpdated(findItem(id), null);
    }

    /** Triggers the onItemRemoved method of any observer. */
    protected void notifyObserversOfRemoval(ContentId id) {
        if (mObserver != null) mObserver.onItemRemoved(id);
    }

    /** @return an offline item with matching {@link ContentId} if it exists and null otherwise. */
    protected OfflineItem findItem(ContentId id) {
        for (OfflineItem item : mItems) {
            if (item.id.equals(id)) {
                return item;
            }
        }
        return null;
    }

    public ArrayList<OfflineItem> getItemsSynchronously() {
        return mItems;
    }
}
