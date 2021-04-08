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
    private String mProactiveSessionId;
    private int mProactiveQueryId;
    private @LensEntryPoint int mLensEntryPoint;

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
        private String mProactiveSessionId;
        private int mProactiveQueryId;
        private @LensEntryPoint int mLensEntryPoint;

        public Builder() {}

        // TODO(b/180967190): remove the with* methods for the required params once
        // downstream references are updated.
        // lensEntryPoint and isIncognito are required params when creating the
        // LensIntentParams.
        public Builder(@LensEntryPoint int lensEntryPoint, boolean isIncognito) {
            this();
            this.mLensEntryPoint = lensEntryPoint;
            this.mIsIncognito = isIncognito;
        }

        /**
         * Sets the Lens entry point.
         *
         * @param lensEntryPoint The entry point to set as a parameter
         */
        public Builder withLensEntryPoint(@LensEntryPoint int lensEntryPoint) {
            this.mLensEntryPoint = lensEntryPoint;
            return this;
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

        /**
         * Build LensIntentParams object from parameters set.
         */
        public LensIntentParams build() {
            LensIntentParams lensIntentParams = new LensIntentParams();
            lensIntentParams.mIsIncognito = mIsIncognito;
            lensIntentParams.mLensEntryPoint = mLensEntryPoint;
            lensIntentParams.mIntentType = mIntentType;
            lensIntentParams.mRequiresConfirmation = mRequiresConfirmation;
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

    /** Returns the requiresConfirmation for this set of params. */
    public boolean getRequiresConfirmation() {
        return mRequiresConfirmation;
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
