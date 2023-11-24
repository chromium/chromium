// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import org.jni_zero.CalledByNative;

import org.chromium.chrome.browser.profiles.OTRProfileID;
import org.chromium.components.download.DownloadState;
import org.chromium.components.download.ResumeMode;
import org.chromium.components.offline_items_collection.ContentId;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.components.offline_items_collection.OfflineItem.Progress;
import org.chromium.components.offline_items_collection.OfflineItemFilter;
import org.chromium.components.offline_items_collection.OfflineItemState;

/**
 * A generic class representing a download item. The item can be either downloaded through the
 * Android DownloadManager, or through Chrome's network stack.
 *
 * This represents the native DownloadItem at a specific point in time -- the native side
 * DownloadManager must be queried for the correct status.
 */
public class DownloadItem {
    private final ContentId mContentId = new ContentId();
    private boolean mUseAndroidDownloadManager;
    private DownloadInfo mDownloadInfo;
    private long mDownloadId = DownloadConstants.INVALID_DOWNLOAD_ID;
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
        OfflineItem offlineItem = new OfflineItem();
        DownloadInfo downloadInfo = item.getDownloadInfo();
        offlineItem.id = downloadInfo.getContentId();
        offlineItem.filePath = downloadInfo.getFilePath();
        offlineItem.title = downloadInfo.getFileName();
        offlineItem.description = downloadInfo.getDescription();
        offlineItem.isTransient = downloadInfo.getIsTransient();
        offlineItem.isAccelerated = downloadInfo.getIsParallelDownload();
        offlineItem.isSuggested = false;
        offlineItem.totalSizeBytes = downloadInfo.getBytesTotalSize();
        offlineItem.receivedBytes = downloadInfo.getBytesReceived();
        offlineItem.isResumable = downloadInfo.isResumable();
        offlineItem.url = downloadInfo.getUrl();
        offlineItem.originalUrl = downloadInfo.getOriginalUrl();
        offlineItem.isOffTheRecord = downloadInfo.isOffTheRecord();
        offlineItem.otrProfileId = OTRProfileID.serialize(downloadInfo.getOTRProfileId());
        offlineItem.mimeType = downloadInfo.getMimeType();
        offlineItem.progress = downloadInfo.getProgress();
        offlineItem.timeRemainingMs = downloadInfo.getTimeRemainingInMillis();
        offlineItem.isDangerous = downloadInfo.getIsDangerous();
        offlineItem.pendingState = downloadInfo.getPendingState();
        offlineItem.failState = downloadInfo.getFailState();
        offlineItem.promoteOrigin = downloadInfo.getShouldPromoteOrigin();
        offlineItem.lastAccessedTimeMs = downloadInfo.getLastAccessTime();
        offlineItem.creationTimeMs = item.getStartTime();
        offlineItem.completionTimeMs = item.getEndTime();
        offlineItem.externallyRemoved = item.hasBeenExternallyRemoved();
        offlineItem.canRename = item.getDownloadInfo().state() == DownloadState.COMPLETE;
        switch (downloadInfo.state()) {
            case DownloadState.IN_PROGRESS:
                offlineItem.state =
                        downloadInfo.isPaused()
                                ? OfflineItemState.PAUSED
                                : OfflineItemState.IN_PROGRESS;
                break;
            case DownloadState.COMPLETE:
                offlineItem.state =
                        downloadInfo.getBytesReceived() == 0
                                ? OfflineItemState.FAILED
                                : OfflineItemState.COMPLETE;
                break;
            case DownloadState.CANCELLED:
                offlineItem.state = OfflineItemState.CANCELLED;
                break;
            case DownloadState.INTERRUPTED:
                @ResumeMode
                int resumeMode =
                        DownloadUtils.getResumeMode(
                                downloadInfo.getUrl().getSpec(), downloadInfo.getFailState());
                if (resumeMode == ResumeMode.INVALID || resumeMode == ResumeMode.USER_RESTART) {
                    // Fail but can restart from the beginning. The UI should let the user to retry.
                    offlineItem.state = OfflineItemState.INTERRUPTED;
                }
                // TODO(xingliu): isDownloadPaused and isDownloadPending rely on isAutoResumable
                // is set correctly in {@link DownloadSharedPreferenceEntry}. The states of
                // notification UI and download home currently may not match. Also pending is
                // related to Java side auto resumption on good network condition.
                else if (downloadInfo.isPaused()) {
                    offlineItem.state = OfflineItemState.PAUSED;
                } else if (DownloadUtils.isDownloadPending(item)) {
                    offlineItem.state = OfflineItemState.PENDING;
                } else {
                    // Unknown failure state.
                    offlineItem.state = OfflineItemState.FAILED;
                }
                break;
            default:
                assert false;
        }

        switch (DownloadFilter.fromMimeType(downloadInfo.getMimeType())) {
            case DownloadFilter.Type.PAGE:
                offlineItem.filter = OfflineItemFilter.PAGE;
                break;
            case DownloadFilter.Type.VIDEO:
                offlineItem.filter = OfflineItemFilter.VIDEO;
                break;
            case DownloadFilter.Type.AUDIO:
                offlineItem.filter = OfflineItemFilter.AUDIO;
                break;
            case DownloadFilter.Type.IMAGE:
                offlineItem.filter = OfflineItemFilter.IMAGE;
                break;
            case DownloadFilter.Type.DOCUMENT:
                offlineItem.filter = OfflineItemFilter.DOCUMENT;
                break;
            case DownloadFilter.Type.OTHER:
            default:
                offlineItem.filter = OfflineItemFilter.OTHER;
                break;
        }

        return offlineItem;
    }

    @CalledByNative
    private static DownloadItem createDownloadItem(
            DownloadInfo downloadInfo,
            long startTimestamp,
            long endTimestamp,
            boolean hasBeenExternallyRemoved) {
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
