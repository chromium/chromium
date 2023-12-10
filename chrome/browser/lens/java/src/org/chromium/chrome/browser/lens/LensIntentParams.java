// Copyright 2020 The Chromium Authors
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
    private int mIntentType;
    private String mProactiveSessionId;
    private int mProactiveQueryId;
    private @LensEntryPoint int mLensEntryPoint;

    /** Builder class for LensIntentParams. */
    public static class Builder {
        private Uri mImageUri = Uri.EMPTY;
        private String mSrcUrl;
        private String mImageTitleOrAltText;
        private String mPageUrl;
        private boolean mIsIncognito;
        private int mIntentType;
        private String mProactiveSessionId;
        private int mProactiveQueryId;
        private @LensEntryPoint int mLensEntryPoint;

        public Builder() {}

        // lensEntryPoint and isIncognito are required params when creating the
        // LensIntentParams.
        public Builder(@LensEntryPoint int lensEntryPoint, boolean isIncognito) {
            this();
            this.mLensEntryPoint = lensEntryPoint;
            this.mIsIncognito = isIncognito;
        }

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
         * Sets the intent type.
         *
         * @param intentType The intent type to set as a parameter
         */
        public Builder withIntentType(int intentType) {
            this.mIntentType = intentType;
            return this;
        }

        /**
         * Optionally set the session id for intents that were triggered
         * by a proactive UI element.
         * @param proactiveSessionId ID distinguishing the session responsible for the intent
         */
        public Builder withProactiveSessionId(String proactiveSessionId) {
            this.mProactiveSessionId = proactiveSessionId;
            return this;
        }

        /**
         * Optionally set the query id for intents that were triggered
         * by a proactive UI element.
         *
         * @param queryId ID distinguishing the query responsible for the intent
         */
        public Builder withProactiveQueryId(int proactiveQueryId) {
            this.mProactiveQueryId = proactiveQueryId;
            return this;
        }

        /** Build LensIntentParams object from parameters set. */
        public LensIntentParams build() {
            LensIntentParams lensIntentParams = new LensIntentParams();
            lensIntentParams.mIsIncognito = mIsIncognito;
            lensIntentParams.mLensEntryPoint = mLensEntryPoint;
            lensIntentParams.mIntentType = mIntentType;
            lensIntentParams.mProactiveSessionId = mProactiveSessionId;
            lensIntentParams.mProactiveQueryId = mProactiveQueryId;
            if (!Uri.EMPTY.equals(mImageUri)) {
                lensIntentParams.mImageUri = mImageUri;
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

    /** Returns the imageUri for this set of params. */
    public Uri getImageUri() {
        return mImageUri;
    }

    /** Returns the pageUrl for this set of params. */
    public String getPageUrl() {
        return mPageUrl;
    }

    /** Returns the srcUrl for this set of params. */
    public String getSrcUrl() {
        return mSrcUrl;
    }

    /** Returns the imageTitleOrAltText for this set of params. */
    public String getImageTitleOrAltText() {
        return mImageTitleOrAltText;
    }

    /** Returns the isIncognito for this set of params. */
    public boolean getIsIncognito() {
        return mIsIncognito;
    }

    /** Returns the intentType for this set of params. */
    public int getIntentType() {
        return mIntentType;
    }

    /** Returns the sessionId for this set of params. */
    public String getProactiveSessionId() {
        return mProactiveSessionId;
    }

    /** Returns the sessionId for this set of params. */
    public int getProactiveQueryId() {
        return mProactiveQueryId;
    }

    /** Returns the {@link LensEntryPoint} for this set of params. */
    public @LensEntryPoint int getLensEntryPoint() {
        return mLensEntryPoint;
    }
}
