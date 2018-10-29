// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.components.offline_items_collection.ContentId;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.components.offline_items_collection.OfflineItem.Progress;

/**
 * A generic class representing a download item. The item can be either downloaded through the
 * Android DownloadManager, or through Chrome's network stack.
 *
 * This represents the native DownloadItem at a specific point in time -- the native side
 * DownloadManager must be queried for the correct status.
 */
public class DownloadItem {
    static final long INVALID_DOWNLOAD_ID = -1L;

    private final ContentId mContentId = new ContentId();
    private boolean mUseAndroidDownloadManager;
    private DownloadInfo mDownloadInfo;
    private long mDownloadId = INVALID_DOWNLOAD_ID;
    private long mStartTime;
    private long mEndTime;
    private boolean mHasBeenExternallyRemoved;

    public DownloadItem(boolean useAndroidDownloadManager, DownloadInfo info) {
        mUseAndroidDownloadManager = useAndroidDownloadManager;
        mDownloadInfo = info;
        if (mDownloadInfo != null) mContentId.namespace = mDownloadInfo.getContentId().namespace;
        mContentId.id = getId();
    }

    /**
     * Sets the system download ID retrieved from Android DownloadManager.
     *
     * @param downloadId ID from the Android DownloadManager.
     */
    public void setSystemDownloadId(long downloadId) {
        mDownloadId = downloadId;

        // Update our ContentId in case it changed.
        mContentId.id = getId();
    }

    /**
     * @return whether the download item has a valid system download ID.
     */
    public boolean hasSystemDownloadId() {
        return mDownloadId != INVALID_DOWNLOAD_ID;
    }

    /**
     * @return System download ID from the Android DownloadManager.
     */
    public long getSystemDownloadId() {
        return mDownloadId;
    }

    /**
     * @return A {@link ContentId} that represents this downloaded item.  The id will match
     *         {@link #getId()}.
     */
    public ContentId getContentId() {
        return mContentId;
    }

    /**
     * @return String ID that uniquely identifies the download.
     */
    public String getId() {
        if (mUseAndroidDownloadManager) {
            return String.valueOf(mDownloadId);
        }
        return mDownloadInfo.getDownloadGuid();
    }

    /**
     * @return Info about the download.
     */
    public DownloadInfo getDownloadInfo() {
        return mDownloadInfo;
    }

    /**
     * Sets the system download info.
     *
     * @param info Download information.
     */
    public void setDownloadInfo(DownloadInfo info) {
        mDownloadInfo = info;
    }

    /**
     * Sets the download start time.
     *
     * @param startTime Download start time from System.currentTimeMillis().
     */
    public void setStartTime(long startTime) {
        mStartTime = startTime;
    }

    /**
     * Gets the download start time.
     *
     * @return Download start time from System.currentTimeMillis().
     */
    public long getStartTime() {
        return mStartTime;
    }

    /**
     * Sets the download end time.
     *
     * @param endTime Download end time from System.currentTimeMillis().
     */
    public void setEndTime(long endTime) {
        mEndTime = endTime;
    }

    /**
     * Gets the download end time.
     *
     * @return Download end time from System.currentTimeMillis().
     */
    public long getEndTime() {
        return mEndTime;
    }

    /**
     * Sets whether the file associated with this item has been removed through an external
     * action.
     *
     * @param hasBeenExternallyRemoved Whether the file associated with this item has been removed
     *                                 from the file system through a means other than the browser
     *                                 download ui.
     */
    public void setHasBeenExternallyRemoved(boolean hasBeenExternallyRemoved) {
        mHasBeenExternallyRemoved = hasBeenExternallyRemoved;
    }

    /**
     * @return Whether the file associated with this item has been removed from the file system
     *         through a means other than the browser download ui.
     */
    public boolean hasBeenExternallyRemoved() {
        return mHasBeenExternallyRemoved;
    }

    /**
     * Helper method to build an {@link OfflineItem} from a {@link DownloadItem}.
     * @param item The {@link DownloadItem} to mimic.
     * @return     A {@link OfflineItem} containing the relevant fields from {@code item}.
     */
    public static OfflineItem createOfflineItem(DownloadItem item) {
        OfflineItem offlineItem = DownloadInfo.createOfflineItem(item.getDownloadInfo());
        offlineItem.creationTimeMs = item.getStartTime();
        offlineItem.completionTimeMs = item.getEndTime();
        offlineItem.externallyRemoved = item.hasBeenExternallyRemoved();
        return offlineItem;
    }

    @CalledByNative
    private static DownloadItem createDownloadItem(DownloadInfo downloadInfo, long startTimestamp,
            long endTimestamp, boolean hasBeenExternallyRemoved) {
        DownloadItem downloadItem = new DownloadItem(false, downloadInfo);
        downloadItem.setStartTime(startTimestamp);
        downloadItem.setEndTime(endTimestamp);
        downloadItem.setHasBeenExternallyRemoved(hasBeenExternallyRemoved);
        return downloadItem;
    }

    /**
     * @return Whether or not the download has an indeterminate percentage.
     */
    public boolean isIndeterminate() {
        Progress progress = getDownloadInfo().getProgress();
        return progress == null || progress.isIndeterminate();
    }
}
