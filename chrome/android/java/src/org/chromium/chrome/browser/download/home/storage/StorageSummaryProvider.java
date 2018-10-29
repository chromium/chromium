// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.storage;

import android.content.Context;
import android.os.Environment;
import android.support.annotation.Nullable;

import org.chromium.base.task.AsyncTask;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.download.DirectoryOption;
import org.chromium.chrome.browser.download.DownloadUtils;
import org.chromium.chrome.browser.download.home.filter.OfflineItemFilterObserver;
import org.chromium.chrome.browser.download.home.filter.OfflineItemFilterSource;
import org.chromium.chrome.browser.download.ui.DownloadHistoryAdapter;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.components.offline_items_collection.OfflineItemState;

import java.io.File;
import java.util.Collection;

/**
 * Provides the storage summary text to be shown inside the download home.
 * TODO(shaktisahu): Rename this class to StorageSummaryMediator and have it manipulate the model
 * directly once migration to new download home is complete.
 */
public class StorageSummaryProvider implements OfflineItemFilterObserver {
    /** A delegate for updating the UI about the storage information. */
    public interface Delegate { void onStorageInfoChanged(String storageInfo); }

    private final Context mContext;
    private final Delegate mDelegate;

    // Contains total space and available space of the file system.
    private DirectoryOption mDirectoryOption;

    // The total size in bytes used by downloads.
    private long mTotalDownloadSize;

    /**
     * Asynchronous task to query the default download directory option on primary storage.
     * Pass one String parameter as the name of the directory option.
     */
    private static class DefaultDirectoryTask extends AsyncTask<DirectoryOption> {
        @Override
        protected DirectoryOption doInBackground() {
            File defaultDownloadDir =
                    Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS);
            DirectoryOption directoryOption =
                    new DirectoryOption("", defaultDownloadDir.getAbsolutePath(),
                            defaultDownloadDir.getUsableSpace(), defaultDownloadDir.getTotalSpace(),
                            DirectoryOption.DownloadLocationDirectoryType.DEFAULT);
            return directoryOption;
        }
    }

    public StorageSummaryProvider(
            Context context, Delegate delegate, @Nullable OfflineItemFilterSource filterSource) {
        mContext = context;
        mDelegate = delegate;

        if (filterSource != null) {
            filterSource.addObserver(this);
            mTotalDownloadSize = getTotalSize(filterSource.getItems());
        }

        computeTotalStorage();
    }

    /**
     * Sets the total size used by downloads. Used to support legacy download home UI, see
     * {@link DownloadHistoryAdapter}.
     * @param totalSize
     */
    public void setUsedStorage(long totalSize) {
        mTotalDownloadSize = totalSize;
        update();
    }

    // OfflineItemFilterObserver implementation.
    @Override
    public void onItemsAdded(Collection<OfflineItem> items) {
        mTotalDownloadSize += getTotalSize(items);
        update();
    }

    @Override
    public void onItemsRemoved(Collection<OfflineItem> items) {
        mTotalDownloadSize -= getTotalSize(items);
        update();
    }

    @Override
    public void onItemUpdated(OfflineItem oldItem, OfflineItem item) {
        // Computes the delta of storage used by downloads.
        mTotalDownloadSize -= oldItem.receivedBytes;
        mTotalDownloadSize += item.receivedBytes;

        if (item.state != OfflineItemState.IN_PROGRESS) update();
    }

    private void computeTotalStorage() {
        DefaultDirectoryTask task = new DefaultDirectoryTask() {
            @Override
            protected void onPostExecute(DirectoryOption directoryOption) {
                mDirectoryOption = directoryOption;
                update();
            }
        };
        task.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    private long getTotalSize(Collection<OfflineItem> items) {
        long totalSize = 0;
        for (OfflineItem item : items) totalSize += item.receivedBytes;
        return totalSize;
    }

    private void update() {
        if (mDirectoryOption == null) return;

        // Build the storage summary string.
        assert(mTotalDownloadSize >= 0);
        String storageSummary = mContext.getString(R.string.download_manager_ui_space_using,
                DownloadUtils.getStringForBytes(mContext, mTotalDownloadSize),
                DownloadUtils.getStringForBytes(mContext, mDirectoryOption.totalSpace));
        mDelegate.onStorageInfoChanged(storageSummary);
    }
}
