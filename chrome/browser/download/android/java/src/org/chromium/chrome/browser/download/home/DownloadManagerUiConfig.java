// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home;

import static org.chromium.components.browser_ui.util.ConversionUtils.BYTES_PER_MEGABYTE;

import android.view.View;

import org.chromium.base.ContextUtils;
import org.chromium.base.SysUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.OtrProfileId;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.edge_to_edge.EdgeToEdgePadAdjuster;

import java.util.function.Function;

/** Provides the configuration params required by the download home UI. */
@NullMarked
public class DownloadManagerUiConfig {
    /** If not null, which off the record items to show in the UI. */
    public final @Nullable OtrProfileId otrProfileId;

    /** Whether or not the UI should be shown as part of a separate activity. */
    public final boolean isSeparateActivity;

    /** Whether generic view types should be used wherever possible. Used for low end devices. */
    public final boolean useGenericViewTypes;

    /** Whether showing full width images should be supported. */
    public final boolean supportFullWidthImages;

    /** The in-memory thumbnail size in bytes. */
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
    public final long justNowThresholdSeconds = 30 * 60;

    /** Whether or not grouping items into a single card is supported. */
    public final boolean supportsGrouping;

    /** Whether or not to show the pagination headers in the list. */
    public final boolean showPaginationHeaders;

    /** Whether or not to start the UI focused on prefetched content. */
    public final boolean startWithPrefetchedContent;

    /**
     * Whether or not items with a Dangerous verdict from Safe Browsing should be shown with warning
     * text/icon in the list.
     */
    public final boolean showDangerousItems;

    /**
     * A generator for the {@link EdgeToEdgePadAdjuster} to be used to adjust the padding for the
     * download manager.
     */
    public final @Nullable Function<View, EdgeToEdgePadAdjuster> edgeToEdgePadAdjusterGenerator;

    /** Whether to show the search bar inline with the content. */
    public final boolean inlineSearchBar;

    /** Whether to auto-focus the search box. */
    public final boolean autoFocusSearchBox;

    /** Constructor. */
    private DownloadManagerUiConfig(Builder builder) {
        otrProfileId = builder.mOtrProfileId;
        isSeparateActivity = builder.mIsSeparateActivity;
        useGenericViewTypes = builder.mUseGenericViewTypes;
        supportFullWidthImages = builder.mSupportFullWidthImages;
        inMemoryThumbnailCacheSizeBytes = builder.mInMemoryThumbnailCacheSizeBytes;
        maxThumbnailScaleFactor = builder.mMaxThumbnailScaleFactor;
        supportsGrouping = builder.mSupportsGrouping;
        showPaginationHeaders = builder.mShowPaginationHeaders;
        startWithPrefetchedContent = builder.mStartWithPrefetchedContent;
        showDangerousItems = builder.mShowDangerousItems;
        inlineSearchBar = builder.mInlineSearchBar;
        autoFocusSearchBox = builder.mAutoFocusSearchBox;
        edgeToEdgePadAdjusterGenerator = builder.mEdgeToEdgePadAdjusterGenerator;
    }

    /** Helper class for building a {@link DownloadManagerUiConfig}. */
    public static class Builder {
        private static final int IN_MEMORY_THUMBNAIL_CACHE_SIZE_BYTES = 15 * BYTES_PER_MEGABYTE;

        private static final float MAX_THUMBNAIL_SCALE_FACTOR = 1.5f; /* hdpi scale factor. */

        private @Nullable OtrProfileId mOtrProfileId;
        private boolean mIsSeparateActivity;
        private boolean mUseGenericViewTypes;
        private boolean mSupportFullWidthImages;
        private int mInMemoryThumbnailCacheSizeBytes = IN_MEMORY_THUMBNAIL_CACHE_SIZE_BYTES;
        private float mMaxThumbnailScaleFactor = MAX_THUMBNAIL_SCALE_FACTOR;
        private boolean mSupportsGrouping;
        private boolean mShowPaginationHeaders;
        private boolean mStartWithPrefetchedContent;
        private boolean mShowDangerousItems;
        private @Nullable Function<View, EdgeToEdgePadAdjuster> mEdgeToEdgePadAdjusterGenerator;
        private boolean mInlineSearchBar;
        private boolean mAutoFocusSearchBox;

        public Builder() {
            mSupportFullWidthImages =
                    !DeviceFormFactor.isNonMultiDisplayContextOnTablet(
                            ContextUtils.getApplicationContext());
            mUseGenericViewTypes = SysUtils.isLowEndDevice();
        }

        public Builder setOtrProfileId(@Nullable OtrProfileId otrProfileId) {
            mOtrProfileId = otrProfileId;
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

        public Builder setInMemoryThumbnailCacheSizeBytes(int inMemoryThumbnailCacheSizeBytes) {
            mInMemoryThumbnailCacheSizeBytes = inMemoryThumbnailCacheSizeBytes;
            return this;
        }

        public Builder setMaxThumbnailScaleFactor(float maxThumbnailScaleFactor) {
            mMaxThumbnailScaleFactor = maxThumbnailScaleFactor;
            return this;
        }

        public Builder setShowPaginationHeaders(boolean showPaginationHeaders) {
            mShowPaginationHeaders = showPaginationHeaders;
            return this;
        }

        public Builder setSupportsGrouping(boolean supportsGrouping) {
            mSupportsGrouping = supportsGrouping;
            return this;
        }

        public Builder setStartWithPrefetchedContent(boolean startWithPrefetchedContent) {
            mStartWithPrefetchedContent = startWithPrefetchedContent;
            return this;
        }

        public Builder setShowDangerousItems(boolean showDangerousItems) {
            mShowDangerousItems = showDangerousItems;
            return this;
        }

        public Builder setEdgeToEdgePadAdjusterGenerator(
                Function<View, EdgeToEdgePadAdjuster> edgeToEdgePadAdjusterGenerator) {
            mEdgeToEdgePadAdjusterGenerator = edgeToEdgePadAdjusterGenerator;
            return this;
        }

        public Builder setInlineSearchBar(boolean inlineSearchBar) {
            mInlineSearchBar = inlineSearchBar;
            return this;
        }

        public Builder setAutoFocusSearchBox(boolean autoFocusSearchBox) {
            mAutoFocusSearchBox = autoFocusSearchBox;
            return this;
        }

        public DownloadManagerUiConfig build() {
            return new DownloadManagerUiConfig(this);
        }
    }
}
