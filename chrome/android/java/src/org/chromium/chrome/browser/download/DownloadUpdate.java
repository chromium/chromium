// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.graphics.Bitmap;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.chrome.browser.profiles.OTRProfileID;
import org.chromium.components.offline_items_collection.ContentId;
import org.chromium.components.offline_items_collection.FailState;
import org.chromium.components.offline_items_collection.OfflineItem.Progress;
import org.chromium.components.offline_items_collection.PendingState;
import org.chromium.url.GURL;

/**
 * Class representing information relating to an update in download status.
 * TODO(crbug.com/40506285): Consolidate with other downloads-related objects.
 */
public final class DownloadUpdate {
    private final ContentId mContentId;
    private final String mFileName;
    private final String mFilePath;
    private final Bitmap mIcon;
    private final int mIconId;
    private final boolean mIsOffTheRecord;
    private final @Nullable OTRProfileID mOTRProfileID;
    private final boolean mIsOpenable;
    private final boolean mIsSupportedMimeType;
    private final boolean mIsTransient;
    private final int mNotificationId;
    private final @NonNull GURL mOriginalUrl;
    private final boolean mShouldPromoteOrigin;
    private final Progress mProgress;
    private final @NonNull GURL mReferrer;
    private final long mStartTime;
    private final long mSystemDownloadId;
    private final long mTimeRemainingInMillis;
    private final long mTotalBytes;
    private final @FailState int mFailState;
    private final @PendingState int mPendingState;

    private DownloadUpdate(Builder builder) {
        this.mContentId = builder.mContentId;
        this.mFileName = builder.mFileName;
        this.mFilePath = builder.mFilePath;
        this.mIcon = builder.mIcon;
        this.mIconId = builder.mIconId;
        this.mIsOffTheRecord = builder.mIsOffTheRecord;
        this.mOTRProfileID = builder.mOTRProfileID;
        this.mIsOpenable = builder.mIsOpenable;
        this.mIsSupportedMimeType = builder.mIsSupportedMimeType;
        this.mIsTransient = builder.mIsTransient;
        this.mNotificationId = builder.mNotificationId;
        this.mOriginalUrl = builder.mOriginalUrl == null ? GURL.emptyGURL() : builder.mOriginalUrl;
        this.mShouldPromoteOrigin = builder.mShouldPromoteOrigin;
        this.mProgress = builder.mProgress;
        this.mReferrer = builder.mReferrer == null ? GURL.emptyGURL() : builder.mReferrer;
        this.mStartTime = builder.mStartTime;
        this.mSystemDownloadId = builder.mSystemDownloadId;
        this.mTimeRemainingInMillis = builder.mTimeRemainingInMillis;
        this.mTotalBytes = builder.mTotalBytes;
        this.mFailState = builder.mFailState;
        this.mPendingState = builder.mPendingState;
    }

    public ContentId getContentId() {
        return mContentId;
    }

    public String getFileName() {
        return mFileName;
    }

    public String getFilePath() {
        return mFilePath;
    }

    public Bitmap getIcon() {
        return mIcon;
    }

    public int getIconId() {
        return mIconId;
    }

    public boolean getIsDownloadPending() {
        return getPendingState() != PendingState.NOT_PENDING;
    }

    public boolean getIsOffTheRecord() {
        return mIsOffTheRecord;
    }

    public @Nullable OTRProfileID getOTRProfileID() {
        return mOTRProfileID;
    }

    public boolean getIsOpenable() {
        return mIsOpenable;
    }

    public boolean getIsSupportedMimeType() {
        return mIsSupportedMimeType;
    }

    public boolean getIsTransient() {
        return mIsTransient;
    }

    public int getNotificationId() {
        return mNotificationId;
    }

    public @NonNull GURL getOriginalUrl() {
        return mOriginalUrl;
    }

    public boolean getShouldPromoteOrigin() {
        return mShouldPromoteOrigin;
    }

    public Progress getProgress() {
        return mProgress;
    }

    public @NonNull GURL getReferrer() {
        return mReferrer;
    }

    public long getStartTime() {
        return mStartTime;
    }

    public long getSystemDownloadId() {
        return mSystemDownloadId;
    }

    public long getTimeRemainingInMillis() {
        return mTimeRemainingInMillis;
    }

    public long getTotalBytes() {
        return mTotalBytes;
    }

    public @FailState int getFailState() {
        return mFailState;
    }

    public @PendingState int getPendingState() {
        return mPendingState;
    }

    /** Helper class for building the DownloadUpdate object. */
    public static class Builder {
        private ContentId mContentId;
        private String mFileName;
        private String mFilePath;
        private Bitmap mIcon;
        private int mIconId = -1;
        private boolean mIsOffTheRecord;
        private @Nullable OTRProfileID mOTRProfileID;
        private boolean mIsOpenable;
        private boolean mIsSupportedMimeType;
        private boolean mIsTransient;
        private int mNotificationId = -1;
        private GURL mOriginalUrl;
        private boolean mShouldPromoteOrigin;
        private Progress mProgress;
        private GURL mReferrer;
        private long mStartTime;
        private long mSystemDownloadId = -1;
        private long mTimeRemainingInMillis;
        private long mTotalBytes;
        private @FailState int mFailState;
        private @PendingState int mPendingState;

        public Builder setContentId(ContentId contentId) {
            this.mContentId = contentId;
            return this;
        }

        public Builder setFileName(String fileName) {
            this.mFileName = fileName;
            return this;
        }

        public Builder setFilePath(String filePath) {
            this.mFilePath = filePath;
            return this;
        }

        public Builder setIcon(Bitmap icon) {
            this.mIcon = icon;
            return this;
        }

        public Builder setIconId(int iconId) {
            this.mIconId = iconId;
            return this;
        }

        public Builder setOTRProfileID(@Nullable OTRProfileID otrProfileID) {
            this.mOTRProfileID = otrProfileID;
            // TODO(crbug.com/40162349): Remove this after replacing |DownloadUpdate#isOffTheRecord|
            // usages with |DownloadUpdate#getOTRProfileID|.
            this.mIsOffTheRecord = OTRProfileID.isOffTheRecord(otrProfileID);
            return this;
        }

        public Builder setIsOpenable(boolean isOpenable) {
            this.mIsOpenable = isOpenable;
            return this;
        }

        public Builder setIsSupportedMimeType(boolean isSupportedMimeType) {
            this.mIsSupportedMimeType = isSupportedMimeType;
            return this;
        }

        public Builder setIsTransient(boolean isTransient) {
            this.mIsTransient = isTransient;
            return this;
        }

        public Builder setNotificationId(int notificationId) {
            this.mNotificationId = notificationId;
            return this;
        }

        public Builder setOriginalUrl(GURL originalUrl) {
            this.mOriginalUrl = originalUrl;
            return this;
        }

        public Builder setShouldPromoteOrigin(boolean shouldPromoteOrigin) {
            this.mShouldPromoteOrigin = shouldPromoteOrigin;
            return this;
        }

        public Builder setProgress(Progress progress) {
            this.mProgress = progress;
            return this;
        }

        public Builder setReferrer(GURL referrer) {
            this.mReferrer = referrer;
            return this;
        }

        public Builder setStartTime(long startTime) {
            this.mStartTime = startTime;
            return this;
        }

        public Builder setSystemDownload(long systemDownload) {
            this.mSystemDownloadId = systemDownload;
            return this;
        }

        public Builder setTimeRemainingInMillis(long timeRemainingInMillis) {
            this.mTimeRemainingInMillis = timeRemainingInMillis;
            return this;
        }

        public Builder setTotalBytes(long totalBytes) {
            this.mTotalBytes = totalBytes;
            return this;
        }

        public Builder setFailState(@FailState int failState) {
            this.mFailState = failState;
            return this;
        }

        public Builder setPendingState(@PendingState int pendingState) {
            this.mPendingState = pendingState;
            return this;
        }

        public DownloadUpdate build() {
            return new DownloadUpdate(this);
        }
    }
}
