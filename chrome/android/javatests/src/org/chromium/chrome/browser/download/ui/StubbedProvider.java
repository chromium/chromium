// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.ui;

import static junit.framework.Assert.assertEquals;

import android.os.Handler;
import android.os.Looper;

import org.chromium.base.Callback;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.components.offline_items_collection.ContentId;
import org.chromium.components.offline_items_collection.LaunchLocation;
import org.chromium.components.offline_items_collection.LegacyHelpers;
import org.chromium.components.offline_items_collection.OfflineContentProvider;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.components.offline_items_collection.OfflineItemFilter;
import org.chromium.components.offline_items_collection.OfflineItemState;
import org.chromium.components.offline_items_collection.RenameResult;
import org.chromium.components.offline_items_collection.ShareCallback;
import org.chromium.components.offline_items_collection.VisualsCallback;

import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Locale;

/** Stubs out backends used by the Download Home UI. */
public class StubbedProvider {
    /** Stubs out the OfflineContentProvider. */
    public class StubbedOfflineContentProvider implements OfflineContentProvider {
        public final CallbackHelper addCallback = new CallbackHelper();
        public final CallbackHelper removeCallback = new CallbackHelper();
        public final CallbackHelper deleteItemCallback = new CallbackHelper();
        public final ArrayList<OfflineItem> items = new ArrayList<>();
        public OfflineContentProvider.Observer observer;

        @Override
        public void addObserver(OfflineContentProvider.Observer addedObserver) {
            // Immediately indicate that the delegate has loaded.
            observer = addedObserver;
            addCallback.notifyCalled();
        }

        @Override
        public void removeObserver(OfflineContentProvider.Observer removedObserver) {
            assertEquals(observer, removedObserver);
            observer = null;
            removeCallback.notifyCalled();
        }

        @Override
        public void getAllItems(Callback<ArrayList<OfflineItem>> callback) {
            mHandler.post(() -> callback.onResult(items));
        }

        @Override
        public void removeItem(ContentId id) {
            for (OfflineItem item : items) {
                if (item.id.equals(id)) {
                    items.remove(item);
                    break;
                }
            }

            mHandler.post(new Runnable() {
                @Override
                public void run() {
                    observer.onItemRemoved(id);
                    deleteItemCallback.notifyCalled();
                }
            });
        }

        @Override
        public void openItem(@LaunchLocation int location, ContentId id) {}
        @Override
        public void pauseDownload(ContentId id) {}
        @Override
        public void resumeDownload(ContentId id, boolean hasUserGesture) {}
        @Override
        public void cancelDownload(ContentId id) {}

        @Override
        public void getItemById(ContentId id, Callback<OfflineItem> callback) {
            mHandler.post(() -> callback.onResult(null));
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
        public void renameItem(
                ContentId id, String name, Callback<Integer /*RenameResult*/> callback) {
            mHandler.post(() -> callback.onResult(RenameResult.SUCCESS));
        }
    }

    private static final long ONE_GIGABYTE = 1024L * 1024L * 1024L;

    private final Handler mHandler;
    private final StubbedOfflineContentProvider mOfflineContentProvider;

    public StubbedProvider() {
        mHandler = new Handler(Looper.getMainLooper());
        mOfflineContentProvider = new StubbedOfflineContentProvider();
    }

    /** Creates a new OfflineItem with pre-defined values. */
    public static OfflineItem createOfflineItem(int which, String date, int filter)
            throws Exception {
        long startTime = dateToEpoch(date);
        int downloadState = OfflineItemState.COMPLETE;
        if (which == 0) {
            return createOfflineItem("offline_guid_1", "https://url.com", downloadState, 0,
                    "page 1", "/data/fake_path/Downloads/first_file", startTime, 1000, filter);
        } else if (which == 1) {
            return createOfflineItem("offline_guid_2", "http://stuff_and_things.com", downloadState,
                    0, "page 2", "/data/fake_path/Downloads/file_two", startTime, 10000, filter);
        } else if (which == 2) {
            return createOfflineItem("offline_guid_3", "https://url.com", downloadState, 100,
                    "page 3", "/data/fake_path/Downloads/3_file", startTime, 100000, filter);
        } else if (which == 3) {
            return createOfflineItem("offline_guid_4", "https://things.com", downloadState, 1024,
                    "page 4", "/data/fake_path/Downloads/4", startTime, ONE_GIGABYTE * 5L, filter);
        } else {
            return null;
        }
    }

    public static OfflineItem createOfflineItem(String guid, String url, int state,
            long downloadProgressBytes, String title, String targetPath, long startTime,
            long totalSize, int filter) {
        OfflineItem offlineItem = new OfflineItem();
        offlineItem.id = new ContentId(LegacyHelpers.LEGACY_OFFLINE_PAGE_NAMESPACE, guid);
        offlineItem.pageUrl = url;
        offlineItem.state = state;
        offlineItem.receivedBytes = downloadProgressBytes;
        offlineItem.title = title;
        offlineItem.filePath = targetPath;
        offlineItem.creationTimeMs = startTime;
        offlineItem.totalSizeBytes = totalSize;
        offlineItem.filter = filter;
        if (filter == OfflineItemFilter.OTHER) {
            offlineItem.canRename = true;
            offlineItem.mimeType = "application/pdf";
        }
        return offlineItem;
    }

    /** Converts a date string to a timestamp. */
    private static long dateToEpoch(String dateStr) throws Exception {
        return new SimpleDateFormat("yyyyMMdd HH:mm", Locale.getDefault()).parse(dateStr).getTime();
    }
}
