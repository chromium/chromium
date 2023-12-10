// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.lens;

import android.net.Uri;

import androidx.annotation.NonNull;

import org.chromium.content_public.browser.WebContents;

/**
 * A wrapper class for the Lens image query params (e.g. used in LensController.queryImage)
 * to provide a more consistent and extensible API.
 */
public class LensQueryParams {
    private Uri mImageUri;
    private String mPageUrl;
    private String mImageTitleOrAltText;
    private String mPageTitle;
    private WebContents mWebContents;
    private String mSrcUrl;
    private boolean mIsIncognito;
    private @LensEntryPoint int mLensEntryPoint;
    private boolean mIsTablet;

    /** Builder class for LensQueryParams. */
    public static class Builder {
        private Uri mImageUri = Uri.EMPTY;
        private String mPageUrl;
        private String mImageTitleOrAltText;
        private String mPageTitle;
        private WebContents mWebContents;
        private String mSrcUrl;
        private boolean mIsIncognito;
        private @LensEntryPoint int mLensEntryPoint;
        private boolean mIsTablet;

        public Builder(@LensEntryPoint int lensEntryPoint, boolean isIncognito, boolean isTablet) {
            this.mLensEntryPoint = lensEntryPoint;
            this.mIsIncognito = isIncognito;
            this.mIsTablet = isTablet;
        }

        /**
         * Sets the image URI.
         *
         * @param imageUri The image URI to set as a parameter
         */
        public Builder withImageUri(@NonNull Uri imageUri) {
            this.mImageUri = imageUri;
            return this;
        }

        /**
         * Sets the URL of the top level frame of the page.
         *
         * @param pageUrl The page URL string to set as a parameter
         */
        public Builder withPageUrl(String pageUrl) {
            this.mPageUrl = pageUrl;
            return this;
        }

        /**
         * Sets the image title or alt text.
         *
         * @param imageTitleOrAltText The image title or alt text to set as a parameter
         */
        public Builder withImageTitleOrAltText(String imageTitleOrAltText) {
            this.mImageTitleOrAltText = imageTitleOrAltText;
            return this;
        }

        /**
         * Sets the title of the top level frame of the page.
         *
         * @param pageTitle The page title string to set as a parameter
         */
        public Builder withPageTitle(String pageTitle) {
            this.mPageTitle = pageTitle;
            return this;
        }

        /**
         * Sets the web contents.
         *
         * @param webContents The web contents to set as a parameter
         */
        public Builder withWebContents(WebContents webContents) {
            this.mWebContents = webContents;
            return this;
        }

        /**
         * Sets the image source URL.
         *
         * @param srcUrl The image source URL string to set as a parameter
         */
        public Builder withSrcUrl(String srcUrl) {
            this.mSrcUrl = srcUrl;
            return this;
        }

        /** Build LensQueryParams object from parameters set. */
        public LensQueryParams build() {
            LensQueryParams lensQueryParams = new LensQueryParams();
            lensQueryParams.mLensEntryPoint = this.mLensEntryPoint;
            lensQueryParams.mImageUri = this.mImageUri;
            lensQueryParams.mPageUrl = this.mPageUrl;
            lensQueryParams.mImageTitleOrAltText = this.mImageTitleOrAltText;
            lensQueryParams.mPageTitle = this.mPageTitle;
            lensQueryParams.mWebContents = this.mWebContents;
            lensQueryParams.mSrcUrl = this.mSrcUrl;
            lensQueryParams.mIsIncognito = this.mIsIncognito;
            lensQueryParams.mIsTablet = this.mIsTablet;
            return lensQueryParams;
        }
    }

    /**
     * Sets the imageUri.
     * With this setter method we can set the imageUri in a retrieve image callback.
     * e.g., LensChipDelegate#getChipRenderParams.
     * @param imageUri The image URI to set as a parameter
     */
    public void setImageUri(@NonNull Uri imageUri) {
        mImageUri = imageUri;
    }

    /** Returns the image URI for this set of params. */
    public Uri getImageUri() {
        return mImageUri;
    }

    /** Returns the page url for this set of params. */
    public String getPageUrl() {
        return mPageUrl;
    }

    /** Returns the image title or alt text for this set of params. */
    public String getImageTitleOrAltText() {
        return mImageTitleOrAltText;
    }

    /** Returns the page title for this set of params. */
    public String getPageTitle() {
        return mPageTitle;
    }

    /** Returns the web contents for this set of params. */
    public WebContents getWebContents() {
        return mWebContents;
    }

    /** Returns the src url for this set of params. */
    public String getSrcUrl() {
        return mSrcUrl;
    }

    /** Returns the isIncognito for this set of params. */
    public boolean getIsIncognito() {
        return mIsIncognito;
    }

    /** Returns the {@link LensEntryPoint} for this set of params. */
    public @LensEntryPoint int getLensEntryPoint() {
        return mLensEntryPoint;
    }

    /** Returns the isTablet for this set of params. */
    public boolean getIsTablet() {
        return mIsTablet;
    }
}
