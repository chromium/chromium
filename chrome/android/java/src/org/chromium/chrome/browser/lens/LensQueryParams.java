// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.lens;

import android.net.Uri;

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

    /**
     * Builder class for LensQueryParams.
     */
    public static class Builder {
        private Uri mImageUri = Uri.EMPTY;
        private String mPageUrl;
        private String mImageTitleOrAltText;
        private String mPageTitle;
        private WebContents mWebContents;
        private String mSrcUrl;
        private boolean mIsIncognito;

        public Builder() {}

        public Builder withImageUri(Uri imageUri) {
            this.mImageUri = imageUri;
            return this;
        }

        public Builder withPageUrl(String pageUrl) {
            this.mPageUrl = pageUrl;
            return this;
        }

        public Builder withImageTitleOrAltText(String imageTitleOrAltText) {
            this.mImageTitleOrAltText = imageTitleOrAltText;
            return this;
        }

        public Builder withPageTitle(String pageTitle) {
            this.mPageTitle = pageTitle;
            return this;
        }

        public Builder withWebContents(WebContents webContents) {
            this.mWebContents = webContents;
            return this;
        }

        public Builder withSrcUrl(String srcUrl) {
            this.mSrcUrl = srcUrl;
            return this;
        }

        public Builder withIsIncognito(boolean isIncognito) {
            this.mIsIncognito = isIncognito;
            return this;
        }

        public LensQueryParams build() {
            LensQueryParams lensQueryParams = new LensQueryParams();
            lensQueryParams.mImageUri = this.mImageUri;
            lensQueryParams.mPageUrl = this.mPageUrl;
            lensQueryParams.mImageTitleOrAltText = this.mImageTitleOrAltText;
            lensQueryParams.mPageTitle = this.mPageTitle;
            lensQueryParams.mWebContents = this.mWebContents;
            lensQueryParams.mSrcUrl = this.mSrcUrl;
            lensQueryParams.mIsIncognito = this.mIsIncognito;
            return lensQueryParams;
        }
    }

    public void setImageUri(Uri imageUri) {
        mImageUri = imageUri;
    }

    public Uri getImageUri() {
        return mImageUri;
    }

    public String getPageUrl() {
        return mPageUrl;
    }

    public String getImageTitleOrAltText() {
        return mImageTitleOrAltText;
    }

    public String getPageTitle() {
        return mPageTitle;
    }

    public WebContents getWebContents() {
        return mWebContents;
    }

    public String getSrcUrl() {
        return mSrcUrl;
    }

    public boolean getIsIncognito() {
        return mIsIncognito;
    }
}