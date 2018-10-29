// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.ui;

import android.content.ComponentName;
import android.text.TextUtils;

import org.chromium.base.VisibleForTesting;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.download.DownloadInfo;
import org.chromium.chrome.browser.download.DownloadItem;
import org.chromium.chrome.browser.download.DownloadMetrics;
import org.chromium.chrome.browser.download.DownloadNotificationService2;
import org.chromium.chrome.browser.download.DownloadUtils;
import org.chromium.chrome.browser.download.home.metrics.FileExtensions;
import org.chromium.chrome.browser.download.items.OfflineContentAggregatorFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.widget.DateDividedAdapter.TimedItem;
import org.chromium.components.download.DownloadState;
import org.chromium.components.offline_items_collection.LaunchLocation;
import org.chromium.components.offline_items_collection.OfflineContentProvider;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.components.offline_items_collection.OfflineItem.Progress;
import org.chromium.components.offline_items_collection.OfflineItemFilter;
import org.chromium.components.offline_items_collection.OfflineItemState;
import org.chromium.components.url_formatter.UrlFormatter;

import java.io.File;

/** Wraps different classes that contain information about downloads. */
public abstract class DownloadHistoryItemWrapper extends TimedItem {
    protected final BackendProvider mBackendProvider;
    protected final ComponentName mComponentName;
    protected File mFile;
    private Long mStableId;
    private boolean mIsDeletionPending;
    private boolean mShouldShowRecentBadge;

    private DownloadHistoryItemWrapper(BackendProvider provider, ComponentName component) {
        mBackendProvider = provider;
        mComponentName = component;
    }

    @Override
    public long getStableId() {
        if (mStableId == null) {
            // Generate a stable ID that combines the timestamp and the download ID.
            mStableId = (long) getId().hashCode();
            mStableId = (mStableId << 32) + (getTimestamp() & 0x0FFFFFFFF);
        }
        return mStableId;
    }

    /** @return Whether the file will soon be deleted. */
    final boolean isDeletionPending() {
        return mIsDeletionPending;
    }

    /** Track whether or not the file will soon be deleted. */
    final void setIsDeletionPending(boolean state) {
        mIsDeletionPending = state;
    }

    /** @return Whether this download should be shown to the user. */
    boolean isVisibleToUser(@DownloadFilter.Type int filter) {
        if (isDeletionPending()) return false;
        return filter == getFilterType() || filter == DownloadFilter.Type.ALL;
    }

    /** Called when this download should be shared. */
    void share() {
        mBackendProvider.getUIDelegate().shareItem(this);
    }

    /**
     * Starts the delete process, which may or may not immediately delete the item or bring up a UI
     * surface first.
     */
    void startRemove() {
        mBackendProvider.getUIDelegate().deleteItem(this);
    }

    /**
     * @return Whether or not this item can be interacted with or not.  This will change based on
     *         the current selection state of the owning list. */
    boolean isInteractive() {
        return !mBackendProvider.getSelectionDelegate().isSelectionEnabled();
    }

    /** @return Item that is being wrapped. */
    abstract Object getItem();

    /**
     * Replaces the item being wrapped with a new one.
     * @return Whether or not the user needs to be informed of changes to the data.
     */
    abstract boolean replaceItem(Object item);

    /** @return ID representing the download. */
    abstract String getId();

    /** @return String showing where the download resides. */
    public abstract String getFilePath();

    /** @return The file where the download resides. */
    public final File getFile() {
        if (mFile == null) mFile = new File(getFilePath());
        return mFile;
    }

    /** @return String to display for the hostname. */
    public final String getDisplayHostname() {
        return UrlFormatter.formatUrlForSecurityDisplayOmitScheme(getUrl());
    }

    /** @return String to display for the file. */
    public abstract String getDisplayFileName();

    /** @return Size of the file. */
    abstract long getFileSize();

    /** @return URL the file was downloaded from. */
    public abstract String getUrl();

    /** @return {@link DownloadFilter} that represents the file type. */
    public abstract @DownloadFilter.Type int getFilterType();

    /** @return The mime type or null if the item doesn't have one. */
    public abstract String getMimeType();

    /** @return The file extension type. See list at the top of the file. */
    public abstract int getFileExtensionType();

    /** @return How much of the download has completed, or null if there is no progress. */
    abstract Progress getDownloadProgress();

    /** @return String indicating the status of the download. */
    abstract String getStatusString();

    /** @return Whether the file for this item has been removed through an external action. */
    abstract boolean hasBeenExternallyRemoved();

    /** @return Whether this download is associated with the off the record profile. */
    abstract boolean isOffTheRecord();

    /** @return Whether the item is an offline page. */
    public abstract boolean isOfflinePage();

    /** @return Whether this item is to be shown in the suggested reading section. */
    abstract boolean isSuggested();

    /** @return Whether the item has been completely downloaded. */
    abstract boolean isComplete();

    /** @return Whether the download is currently paused. */
    abstract boolean isPaused();

    /** @return Whether the download is currently pending. */
    abstract boolean isPending();

    /** Called when the user wants to open the file. */
    abstract void open();

    /** Called when the user tries to cancel downloading the file. */
    abstract void cancel();

    /** Called when the user tries to pause downloading the file. */
    abstract void pause();

    /** Called when the user tries to resume downloading the file. */
    abstract void resume();

    /**
     * Called when the user wants to remove the download from the backend.
     * May also delete the file associated with the download item.
     *
     * @return Whether the file associated with the download item was deleted.
     */
    abstract boolean removePermanently();

    /** @return Whether this item should be marked as NEW in download home. */
    public boolean shouldShowRecentBadge() {
        return mShouldShowRecentBadge;
    }

    /** Set whether this item should be badged as NEW addition. */
    public void setShouldShowRecentBadge(boolean shouldShowRecentBadge) {
        mShouldShowRecentBadge = shouldShowRecentBadge;
    }

    protected void recordOpenSuccess() {
        RecordUserAction.record("Android.DownloadManager.Item.OpenSucceeded");
        RecordHistogram.recordEnumeratedHistogram("Android.DownloadManager.Item.OpenSucceeded",
                getFilterType(), DownloadFilter.Type.NUM_ENTRIES);

        if (getFilterType() == DownloadFilter.Type.OTHER) {
            RecordHistogram.recordEnumeratedHistogram(
                    "Android.DownloadManager.OtherExtensions.OpenSucceeded", getFileExtensionType(),
                    FileExtensions.Type.NUM_ENTRIES);
        }
    }

    protected void recordOpenFailure() {
        RecordHistogram.recordEnumeratedHistogram("Android.DownloadManager.Item.OpenFailed",
                getFilterType(), DownloadFilter.Type.NUM_ENTRIES);

        if (getFilterType() == DownloadFilter.Type.OTHER) {
            RecordHistogram.recordEnumeratedHistogram(
                    "Android.DownloadManager.OtherExtensions.OpenFailed", getFileExtensionType(),
                    FileExtensions.Type.NUM_ENTRIES);
        }
    }

    /** Wraps a {@link DownloadItem}. */
    public static class DownloadItemWrapper extends DownloadHistoryItemWrapper {
        private DownloadItem mItem;
        private Integer mFileExtensionType;

        DownloadItemWrapper(DownloadItem item, BackendProvider provider, ComponentName component) {
            super(provider, component);
            mItem = item;
        }

        @Override
        public DownloadItem getItem() {
            return mItem;
        }

        @Override
        public boolean replaceItem(Object item) {
            DownloadItem downloadItem = (DownloadItem) item;
            assert TextUtils.equals(mItem.getId(), downloadItem.getId());

            boolean visuallyChanged = isNewItemVisiblyDifferent(downloadItem);
            mItem = downloadItem;
            mFile = null;
            return visuallyChanged;
        }

        @Override
        public String getId() {
            return mItem.getId();
        }

        @Override
        public long getTimestamp() {
            return mItem.getStartTime();
        }

        @Override
        public String getFilePath() {
            return mItem.getDownloadInfo().getFilePath();
        }

        @Override
        public String getDisplayFileName() {
            return mItem.getDownloadInfo().getFileName();
        }

        @Override
        public long getFileSize() {
            if (mItem.getDownloadInfo().state() == DownloadState.COMPLETE) {
                return mItem.getDownloadInfo().getBytesReceived();
            } else {
                return 0;
            }
        }

        @Override
        public String getUrl() {
            return mItem.getDownloadInfo().getUrl();
        }

        @Override
        public int getFilterType() {
            return DownloadFilter.fromMimeType(getMimeType());
        }

        @Override
        public String getMimeType() {
            return mItem.getDownloadInfo().getMimeType();
        }

        @Override
        public int getFileExtensionType() {
            if (mFileExtensionType == null) {
                mFileExtensionType = FileExtensions.getExtension(getFilePath());
            }

            return mFileExtensionType;
        }

        @Override
        public Progress getDownloadProgress() {
            return mItem.getDownloadInfo().getProgress();
        }

        @Override
        public String getStatusString() {
            return DownloadUtils.getStatusString(mItem);
        }

        @Override
        public void open() {
            if (DownloadUtils.openFile(getFile(), getMimeType(),
                        mItem.getDownloadInfo().getDownloadGuid(), isOffTheRecord(),
                        mItem.getDownloadInfo().getOriginalUrl(),
                        mItem.getDownloadInfo().getReferrer(),
                        DownloadMetrics.DownloadOpenSource.DOWNLOAD_HOME)) {
                recordOpenSuccess();
                DownloadMetrics.recordDownloadViewRetentionTime(
                        getMimeType(), getItem().getStartTime());
            } else {
                recordOpenFailure();
            }
        }

        @Override
        public void cancel() {
            mBackendProvider.getDownloadDelegate().broadcastDownloadAction(
                    mItem, DownloadNotificationService2.ACTION_DOWNLOAD_CANCEL);
        }

        @Override
        public void pause() {
            mBackendProvider.getDownloadDelegate().broadcastDownloadAction(
                    mItem, DownloadNotificationService2.ACTION_DOWNLOAD_PAUSE);
        }

        @Override
        public void resume() {
            mBackendProvider.getDownloadDelegate().broadcastDownloadAction(
                    mItem, DownloadNotificationService2.ACTION_DOWNLOAD_RESUME);
        }

        @Override
        public boolean removePermanently() {
            // Tell the DownloadManager to remove the file from history.
            mBackendProvider.getDownloadDelegate().removeDownload(
                    getId(), isOffTheRecord(), hasBeenExternallyRemoved());
            mBackendProvider.getThumbnailProvider().removeThumbnailsFromDisk(getId());
            return false;
        }

        @Override
        boolean hasBeenExternallyRemoved() {
            return mItem.hasBeenExternallyRemoved();
        }

        @Override
        boolean isOffTheRecord() {
            return mItem.getDownloadInfo().isOffTheRecord();
        }

        @Override
        public boolean isOfflinePage() {
            return false;
        }

        @Override
        public boolean isSuggested() {
            return false;
        }

        @Override
        public boolean isComplete() {
            return mItem.getDownloadInfo().state() == DownloadState.COMPLETE;
        }

        @Override
        public boolean isPaused() {
            return DownloadUtils.isDownloadPaused(mItem);
        }

        @Override
        public boolean isPending() {
            return DownloadUtils.isDownloadPending(mItem);
        }

        @Override
        boolean isVisibleToUser(@DownloadFilter.Type int filter) {
            if (!super.isVisibleToUser(filter)) return false;

            if (TextUtils.isEmpty(getFilePath()) || TextUtils.isEmpty(getDisplayFileName())) {
                return false;
            }

            int state = mItem.getDownloadInfo().state();
            if ((state == DownloadState.INTERRUPTED && !mItem.getDownloadInfo().isResumable())
                    || state == DownloadState.CANCELLED) {
                // Mocks don't include showing cancelled/unresumable downloads.  Might need to if
                // undeletable files become a big issue.
                return false;
            }

            return true;
        }

        /** @return whether the given DownloadItem is visibly different from the current one. */
        private boolean isNewItemVisiblyDifferent(DownloadItem newItem) {
            DownloadInfo oldInfo = mItem.getDownloadInfo();
            DownloadInfo newInfo = newItem.getDownloadInfo();

            if (oldInfo.getProgress().equals(newInfo.getProgress())) return true;
            if (oldInfo.getBytesReceived() != newInfo.getBytesReceived()) return true;
            if (oldInfo.state() != newInfo.state()) return true;
            if (oldInfo.isPaused() != newInfo.isPaused()) return true;
            if (!TextUtils.equals(oldInfo.getFilePath(), newInfo.getFilePath())) return true;

            return false;
        }
    }

    /** Wraps a {@link OfflineItem}. */
    public static class OfflineItemWrapper extends DownloadHistoryItemWrapper {
        private OfflineItem mItem;

        OfflineItemWrapper(OfflineItem item, BackendProvider provider, ComponentName component) {
            super(provider, component);
            mItem = item;
        }

        @VisibleForTesting
        public static OfflineItemWrapper createOfflineItemWrapperForTest(OfflineItem item) {
            return new OfflineItemWrapper(item, null, null);
        }

        @Override
        public OfflineItem getItem() {
            return mItem;
        }

        @Override
        public boolean replaceItem(Object item) {
            OfflineItem newItem = (OfflineItem) item;
            assert mItem.id.equals(newItem.id);

            mItem = newItem;
            mFile = null;
            return true;
        }

        @Override
        public String getId() {
            // TODO(shaktisahu): May be change this to mItem.id.toString().
            return mItem.id.id;
        }

        @Override
        public long getTimestamp() {
            return mItem.creationTimeMs;
        }

        @Override
        public String getFilePath() {
            return mItem.filePath;
        }

        @Override
        public String getDisplayFileName() {
            return mItem.title;
        }

        @Override
        public long getFileSize() {
            return mItem.totalSizeBytes;
        }

        @Override
        public String getUrl() {
            return mItem.pageUrl;
        }

        @Override
        public @DownloadFilter.Type int getFilterType() {
            // TODO(shaktisahu): Make DownloadFilter unnecessary.
            return isOfflinePage() ? DownloadFilter.Type.PAGE
                                   : DownloadFilter.fromMimeType(mItem.mimeType);
        }

        @OfflineItemFilter
        public int getOfflineItemFilter() {
            return mItem.filter;
        }

        @Override
        public String getMimeType() {
            return mItem.mimeType;
        }

        @Override
        public int getFileExtensionType() {
            // TODO(shaktisahu): Fix this.
            return FileExtensions.Type.OTHER;
        }

        @Override
        public Progress getDownloadProgress() {
            return mItem.progress;
        }

        @Override
        public String getStatusString() {
            return isOfflinePage() ? DownloadUtils.getOfflinePageStatusString(mItem) : "";
        }

        private OfflineContentProvider getOfflineContentProvider() {
            return OfflineContentAggregatorFactory.forProfile(
                    Profile.getLastUsedProfile().getOriginalProfile());
        }

        @Override
        public void open() {
            getOfflineContentProvider().openItem(LaunchLocation.DOWNLOAD_HOME, mItem.id);
            recordOpenSuccess();
        }

        @Override
        public void cancel() {
            getOfflineContentProvider().cancelDownload(mItem.id);
        }

        @Override
        public void pause() {
            getOfflineContentProvider().pauseDownload(mItem.id);
        }

        @Override
        public void resume() {
            getOfflineContentProvider().resumeDownload(mItem.id, true);
        }

        @Override
        public boolean removePermanently() {
            getOfflineContentProvider().removeItem(mItem.id);
            return true;
        }

        @Override
        boolean hasBeenExternallyRemoved() {
            return mItem.externallyRemoved;
        }

        @Override
        boolean isOffTheRecord() {
            return mItem.isOffTheRecord;
        }

        @Override
        public boolean isOfflinePage() {
            return mItem.filter == OfflineItemFilter.FILTER_PAGE;
        }

        @Override
        public boolean isSuggested() {
            return mItem.isSuggested;
        }

        @Override
        public boolean isComplete() {
            return mItem.state == OfflineItemState.COMPLETE;
        }

        @Override
        public boolean isPaused() {
            return mItem.state == OfflineItemState.PAUSED;
        }

        @Override
        public boolean isPending() {
            return mItem.state == OfflineItemState.PENDING;
        }
    }
}
