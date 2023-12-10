// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.storage;

import android.content.Context;

import androidx.annotation.Nullable;

import org.chromium.base.task.AsyncTask;
import org.chromium.chrome.browser.download.DirectoryOption;
import org.chromium.chrome.browser.download.DownloadDirectoryProvider;
import org.chromium.chrome.browser.download.home.filter.OfflineItemFilterObserver;
import org.chromium.chrome.browser.download.home.filter.OfflineItemFilterSource;
import org.chromium.chrome.browser.download.internal.R;
import org.chromium.components.browser_ui.util.DownloadUtils;
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
    public interface Delegate {
        void onStorageInfoChanged(String storageInfo);
    }

    private final Context mContext;
    private final Delegate mDelegate;

    // Contains total space and available space of the file system.
    private DirectoryOption mDirectoryOption;

    // The total size in bytes used by downloads.
    private long mTotalDownloadSize;

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
        // Asynchronous task to query the default download directory option on primary storage.
        new AsyncTask<DirectoryOption>() {
            @Override
            protected DirectoryOption doInBackground() {
                File defaultDownloadDir = DownloadDirectoryProvider.getPrimaryDownloadDirectory();
                if (defaultDownloadDir == null) return null;

                DirectoryOption directoryOption =
                        new DirectoryOption(
                                "",
                                defaultDownloadDir.getAbsolutePath(),
                                defaultDownloadDir.getUsableSpace(),
                                defaultDownloadDir.getTotalSpace(),
                                DirectoryOption.DownloadLocationDirectoryType.DEFAULT);
                return directoryOption;
            }

            @Override
            protected void onPostExecute(DirectoryOption directoryOption) {
                mDirectoryOption = directoryOption;
                update();
            }
        }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    private long getTotalSize(Collection<OfflineItem> items) {
        long totalSize = 0;
        for (OfflineItem item : items) totalSize += item.receivedBytes;
        return totalSize;
    }

    private void update() {
        if (mDirectoryOption == null) return;

        // Build the storage summary string.
        assert (mTotalDownloadSize >= 0);
        String storageSummary =
                mContext.getString(
                        R.string.download_manager_ui_space_using,
                        DownloadUtils.getStringForBytes(mContext, mTotalDownloadSize),
                        DownloadUtils.getStringForBytes(mContext, mDirectoryOption.totalSpace));
        mDelegate.onStorageInfoChanged(storageSummary);
    }
}
