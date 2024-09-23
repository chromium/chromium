// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;

import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.url.GURL;

/**
 * A container object for passing share extras not contained in {@link ShareParams} to {@link
 * ShareDelegate}.
 *
 * <p>This class contains extras that are used only by Android Share, and should never be
 * componentized. {@link ShareParams} lives in //components and only contains parameters that are
 * used in more than one part of the Chromium codebase.
 *
 * <p>These int value for these enums are safe to modify as its not used for metrics.
 */
public class ChromeShareExtras {
    @IntDef({
        DetailedContentType.NOT_SPECIFIED,
        DetailedContentType.IMAGE,
        DetailedContentType.GIF,
        DetailedContentType.HIGHLIGHTED_TEXT,
        DetailedContentType.SCREENSHOT,
        DetailedContentType.WEB_SHARE,
        DetailedContentType.PAGE_INFO,
    })
    public @interface DetailedContentType {
        int NOT_SPECIFIED = 0;
        int IMAGE = 1;
        int GIF = 2;
        int HIGHLIGHTED_TEXT = 3;
        int SCREENSHOT = 4;
        int WEB_SHARE = 5;
        int PAGE_INFO = 6;
    }

    /** Whether to save the chosen activity for future direct sharing. */
    private final boolean mSaveLastUsed;

    /**
     * Whether it should share directly with the activity that was most recently used to share. If
     * false, the share selection will be saved.
     */
    private final boolean mShareDirectly;

    /** Whether the URL is of the current visible page. */
    private final boolean mIsUrlOfVisiblePage;

    /** Source URL of the image. */
    @NonNull private final GURL mImageSrcUrl;

    /** Url of the content being shared. */
    @NonNull private final GURL mContentUrl;

    private final boolean mIsReshareHighlightedText;

    /** Whether page sharing 1P actions should be added to the share sheet or not. */
    private final boolean mSkipPageSharingActions;

    private final RenderFrameHost mRenderFrameHost;

    /** The detailed content type that is being shared. */
    @DetailedContentType private final int mDetailedContentType;

    private ChromeShareExtras(
            boolean saveLastUsed,
            boolean shareDirectly,
            boolean isUrlOfVisiblePage,
            GURL imageSrcUrl,
            GURL contentUrl,
            boolean isReshareHighlightedText,
            boolean skipPageSharingActions,
            RenderFrameHost renderFrameHost,
            @DetailedContentType int detailedContentType) {
        mSaveLastUsed = saveLastUsed;
        mShareDirectly = shareDirectly;
        mIsUrlOfVisiblePage = isUrlOfVisiblePage;
        mImageSrcUrl = imageSrcUrl == null ? GURL.emptyGURL() : imageSrcUrl;
        mContentUrl = contentUrl == null ? GURL.emptyGURL() : contentUrl;
        mIsReshareHighlightedText = isReshareHighlightedText;
        mSkipPageSharingActions = skipPageSharingActions;
        mRenderFrameHost = renderFrameHost;
        mDetailedContentType = detailedContentType;
    }

    /**
     * @return Whether to save the chosen activity for future direct sharing.
     */
    public boolean saveLastUsed() {
        return mSaveLastUsed;
    }

    /**
     * @return Whether it should share directly with the activity that was most recently used to
     * share.
     */
    public boolean shareDirectly() {
        return mShareDirectly;
    }

    /**
     * @return Whether the URL is of the current visible page.
     */
    public boolean isUrlOfVisiblePage() {
        return mIsUrlOfVisiblePage;
    }

    /**
     * @return Source URL of the image.
     */
    public @NonNull GURL getImageSrcUrl() {
        return mImageSrcUrl;
    }

    /**
     * @return URL of the content being shared.
     */
    public @NonNull GURL getContentUrl() {
        return mContentUrl;
    }

    public boolean isReshareHighlightedText() {
        return mIsReshareHighlightedText;
    }

    /**
     * @return Whether page sharing 1P actions should be added to the share
     * sheet or not.
     */
    public boolean skipPageSharingActions() {
        return mSkipPageSharingActions;
    }

    /**
     * @return The {@link RenderFrameHost} that opened the context menu for sharing.
     */
    public RenderFrameHost getRenderFrameHost() {
        return mRenderFrameHost;
    }

    /**
     * @return The {@link DetailedContentType} of the content that is being shared.
     */
    public int getDetailedContentType() {
        return mDetailedContentType;
    }

    /**
     * Whether the content being shared is an image based on the {@link #getDetailedContentType()}.
     * */
    public boolean isImage() {
        return mDetailedContentType == DetailedContentType.IMAGE
                || mDetailedContentType == DetailedContentType.GIF
                || mDetailedContentType == DetailedContentType.SCREENSHOT;
    }

    /** The builder for {@link ChromeShareExtras} objects. */
    public static class Builder {
        private boolean mSaveLastUsed;
        private boolean mShareDirectly;
        private boolean mIsUrlOfVisiblePage;
        private GURL mImageSrcUrl;
        private GURL mContentUrl;
        private boolean mIsReshareHighlightedText;
        private boolean mSkipPageSharingActions;
        private RenderFrameHost mRenderFrameHost;
        @DetailedContentType private int mDetailedContentType;

        /** Sets whether to save the chosen activity for future direct sharing. */
        public Builder setSaveLastUsed(boolean saveLastUsed) {
            mSaveLastUsed = saveLastUsed;
            return this;
        }

        /** Sets {@link RenderFrameHost} that opened the context menu for sharing. */
        public Builder setRenderFrameHost(RenderFrameHost renderFrameHost) {
            mRenderFrameHost = renderFrameHost;
            return this;
        }

        /**
         * Sets whether it should share directly with the activity that was most recently used to
         * share.
         */
        public Builder setShareDirectly(boolean shareDirectly) {
            mShareDirectly = shareDirectly;
            return this;
        }

        /** Sets whether the URL is of the current visible page. */
        public Builder setIsUrlOfVisiblePage(boolean isUrlOfVisiblePage) {
            mIsUrlOfVisiblePage = isUrlOfVisiblePage;
            return this;
        }

        /** Sets source URL of the image. */
        public Builder setImageSrcUrl(GURL imageSrcUrl) {
            mImageSrcUrl = imageSrcUrl;
            return this;
        }

        /** Sets the URL of the content being shared. */
        public Builder setContentUrl(GURL contentUrl) {
            mContentUrl = contentUrl;
            return this;
        }

        public Builder setIsReshareHighlightedText(boolean isReshareHighlightedText) {
            mIsReshareHighlightedText = isReshareHighlightedText;
            return this;
        }

        public Builder setSkipPageSharingActions(boolean skipPageSharingActions) {
            mSkipPageSharingActions = skipPageSharingActions;
            return this;
        }

        /** Sets the {@link DetailedContentType} of the content that is being shared. */
        public Builder setDetailedContentType(@DetailedContentType int detailedContentType) {
            mDetailedContentType = detailedContentType;
            return this;
        }

        public ChromeShareExtras build() {
            return new ChromeShareExtras(
                    mSaveLastUsed,
                    mShareDirectly,
                    mIsUrlOfVisiblePage,
                    mImageSrcUrl,
                    mContentUrl,
                    mIsReshareHighlightedText,
                    mSkipPageSharingActions,
                    mRenderFrameHost,
                    mDetailedContentType);
        }
    }
}
