// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.lens;

import android.net.Uri;

/**
 * A wrapper class for the Lens intent params (e.g. used in LensController.startLens)
 * to provide a more consistent and extensible API.
 */
public class LensIntentParams {
    private Uri mImageUri;
    private String mSrcUrl;
    private String mImageTitleOrAltText;
    private String mPageUrl;
    private boolean mIsIncognito;
    private boolean mRequiresConfirmation;
    private int mIntentType;

    /**
     * Builder class for LensIntentParams.
     */
    public static class Builder {
        private Uri mImageUri = Uri.EMPTY;
        private String mSrcUrl;
        private String mImageTitleOrAltText;
        private String mPageUrl;
        private boolean mIsIncognito;
        private boolean mRequiresConfirmation;
        private int mIntentType;

        public Builder() {}

        /**
         * Sets the image URI.
         *
         * @param imageUri The image URI to set as a parameter
         */
        public Builder withImageUri(Uri imageUri) {
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
         * Sets the image source URL.
         *
         * @param srcUrl The image source URL string to set as a parameter
         */
        public Builder withSrcUrl(String srcUrl) {
            this.mSrcUrl = srcUrl;
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
         * Sets whether the client is incognito.
         *
         * @param isIncognito Whether the client is incognito as a boolean parameter
         */
        public Builder withIsIncognito(boolean isIncognito) {
            this.mIsIncognito = isIncognito;
            return this;
        }

        /**
         * Sets whether the client requires account confirmation.
         *
         * @param requiresConfirmation Whether the client requires account confirmation as a boolean
         *         parameter
         */
        public Builder withRequiresConfirmation(boolean requiresConfirmation) {
            this.mRequiresConfirmation = requiresConfirmation;
            return this;
        }

        /**
         * Sets the intent type.
         *
         * @param intentType The intent type to set as a parameter
         */
        public Builder withIntentType(int intentType) {
            this.mIntentType = intentType;
            return this;
        }

        /**
         * Build LensIntentParams object from parameters set.
         */
        public LensIntentParams build() {
            LensIntentParams lensIntentParams = new LensIntentParams();
            if (!Uri.EMPTY.equals(mImageUri)) {
                lensIntentParams.mImageUri = mImageUri;
                lensIntentParams.mIsIncognito = mIsIncognito;
                lensIntentParams.mIntentType = mIntentType;
                lensIntentParams.mRequiresConfirmation = mRequiresConfirmation;
                if (mSrcUrl != null) {
                    lensIntentParams.mSrcUrl = mSrcUrl;
                }
                if (mImageTitleOrAltText != null) {
                    lensIntentParams.mImageTitleOrAltText = mImageTitleOrAltText;
                }
                if (mPageUrl != null) {
                    lensIntentParams.mPageUrl = mPageUrl;
                }
            }
            return lensIntentParams;
        }
    }

    /** Retrieve the image URI for the intent. */
    public Uri getImageUri() {
        return mImageUri;
    }

    /** Retrieve the page URL for the intent. */
    public String getPageUrl() {
        return mPageUrl;
    }

    /** Retrieve the image source URL for the intent. */
    public String getSrcUrl() {
        return mSrcUrl;
    }

    /** Retrieve the image title or alt text for the intent. */
    public String getImageTitleOrAltText() {
        return mImageTitleOrAltText;
    }

    /** Retrieve whether the client is incognito for the intent. */
    public boolean getIsIncognito() {
        return mIsIncognito;
    }

    /** Retrieve whether the client requires account for the intent. */
    public boolean getRequiresConfirmation() {
        return mRequiresConfirmation;
    }

    /** Retrieve the intent type. */
    public int getIntentType() {
        return mIntentType;
    }
}
