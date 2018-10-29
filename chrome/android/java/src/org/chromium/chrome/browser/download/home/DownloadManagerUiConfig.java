// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home;

import static org.chromium.chrome.browser.util.ConversionUtils.BYTES_PER_MEGABYTE;

import org.chromium.base.ContextUtils;
import org.chromium.base.SysUtils;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.ui.base.DeviceFormFactor;

/** Provides the configuration params required by the download home UI. */
public class DownloadManagerUiConfig {
    /** Whether or not the UI should include off the record items. */
    public final boolean isOffTheRecord;

    /** Whether or not the UI should be shown as part of a separate activity. */
    public final boolean isSeparateActivity;

    /** Whether generic view types should be used wherever possible. Used for low end devices. */
    public final boolean useGenericViewTypes;

    /** Whether showing full width images should be supported. */
    public final boolean supportFullWidthImages;

    /** Whether or not to use the legacy download path or use the new OfflineContentProvider. */
    public final boolean useNewDownloadPath;

    /**
     * Whether or not to use the legacy download thumbnail path or use the new
     * OfflineContentProvider.
     */
    public final boolean useNewDownloadPathThumbnails;

    /**
     * The in-memory thumbnail size in bytes.
     */
    public final int inMemoryThumbnailCacheSizeBytes;

    /**
     * The maximum thumbnail scale factor, thumbnail on higher dpi devices will downscale the
     * quality to this level.
     */
    public final float maxThumbnailScaleFactor;

    /**
     * The time interval during which a download update is considered recent enough to show
     * in Just Now section.
     */
    public final long justNowThresholdSeconds;

    /** Constructor. */
    private DownloadManagerUiConfig(Builder builder) {
        isOffTheRecord = builder.mIsOffTheRecord;
        isSeparateActivity = builder.mIsSeparateActivity;
        useGenericViewTypes = builder.mUseGenericViewTypes;
        supportFullWidthImages = builder.mSupportFullWidthImages;
        useNewDownloadPath = builder.mUseNewDownloadPath;
        useNewDownloadPathThumbnails = builder.mUseNewDownloadPathThumbnails;
        inMemoryThumbnailCacheSizeBytes = builder.mInMemoryThumbnailCacheSizeBytes;
        maxThumbnailScaleFactor = builder.mMaxThumbnailScaleFactor;
        justNowThresholdSeconds = builder.mJustNowThresholdSeconds;
    }

    /** Helper class for building a {@link DownloadManagerUiConfig}. */
    public static class Builder {
        private static final String JUST_NOW_THRESHOLD_SECONDS_PARAM = "just_now_threshold";

        /** Default value for threshold time interval to show up in Just Now section. */
        private static final int JUST_NOW_THRESHOLD_SECONDS_DEFAULT = 30 * 60;

        private static final int IN_MEMORY_THUMBNAIL_CACHE_SIZE_BYTES = 15 * BYTES_PER_MEGABYTE;

        private boolean mIsOffTheRecord;
        private boolean mIsSeparateActivity;
        private boolean mUseGenericViewTypes;
        private boolean mSupportFullWidthImages;
        private boolean mUseNewDownloadPath;
        private boolean mUseNewDownloadPathThumbnails;
        private int mInMemoryThumbnailCacheSizeBytes = IN_MEMORY_THUMBNAIL_CACHE_SIZE_BYTES;
        private float mMaxThumbnailScaleFactor = 1.f; /* mdpi scale factor. */
        private long mJustNowThresholdSeconds;

        public Builder() {
            readParamsFromFinch();
            mSupportFullWidthImages = !DeviceFormFactor.isNonMultiDisplayContextOnTablet(
                    ContextUtils.getApplicationContext());
            mUseGenericViewTypes = SysUtils.isLowEndDevice();
        }

        public Builder setIsOffTheRecord(boolean isOffTheRecord) {
            mIsOffTheRecord = isOffTheRecord;
            return this;
        }

        public Builder setIsSeparateActivity(boolean isSeparateActivity) {
            mIsSeparateActivity = isSeparateActivity;
            return this;
        }

        public Builder setUseGenericViewTypes(boolean useGenericViewTypes) {
            mUseGenericViewTypes = useGenericViewTypes;
            return this;
        }

        public Builder setSupportFullWidthImages(boolean supportFullWidthImages) {
            mSupportFullWidthImages = supportFullWidthImages;
            return this;
        }

        public Builder setUseNewDownloadPath(boolean useNewDownloadPath) {
            mUseNewDownloadPath = useNewDownloadPath;
            return this;
        }

        public Builder setUseNewDownloadPathThumbnails(boolean useNewDownloadPathThumbnails) {
            mUseNewDownloadPathThumbnails = useNewDownloadPathThumbnails;
            return this;
        }

        public Builder setInMemoryThumbnailCacheSizeBytes(int inMemoryThumbnailCacheSizeBytes) {
            mInMemoryThumbnailCacheSizeBytes = inMemoryThumbnailCacheSizeBytes;
            return this;
        }

        public Builder setMaxThumbnailScaleFactor(float maxThumbnailScaleFactor) {
            mMaxThumbnailScaleFactor = maxThumbnailScaleFactor;
            return this;
        }

        public DownloadManagerUiConfig build() {
            return new DownloadManagerUiConfig(this);
        }

        private void readParamsFromFinch() {
            mJustNowThresholdSeconds = ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                    ChromeFeatureList.DOWNLOAD_HOME_V2, JUST_NOW_THRESHOLD_SECONDS_PARAM,
                    JUST_NOW_THRESHOLD_SECONDS_DEFAULT);
        }
    }
}
