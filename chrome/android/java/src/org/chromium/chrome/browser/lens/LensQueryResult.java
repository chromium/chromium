// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.lens;

/**
 * A wrapper class for the Lens image query result from Lens Prime SDK.
 */
public class LensQueryResult {
    private boolean mIsShoppyIntent;
    private boolean mIsTranslateIntent;
    private int mLensIntentType;

    /**
     * Builder class for LensQueryParams.
     */
    public static class Builder {
        private boolean mIsShoppyIntent;
        private boolean mIsTranslateIntent;
        private int mLensIntentType;

        public Builder() {}

        public Builder withIsShoppyIntent(boolean isShoppyIntent) {
            this.mIsShoppyIntent = isShoppyIntent;
            return this;
        }

        public Builder withIsTranslateIntent(boolean isTranslateIntent) {
            this.mIsTranslateIntent = isTranslateIntent;
            return this;
        }

        public Builder withLensIntentType(int lensIntentType) {
            this.mLensIntentType = lensIntentType;
            return this;
        }

        public LensQueryResult build() {
            LensQueryResult lensQueryResult = new LensQueryResult();
            lensQueryResult.mIsShoppyIntent = this.mIsShoppyIntent;
            lensQueryResult.mIsTranslateIntent = this.mIsTranslateIntent;
            lensQueryResult.mLensIntentType = this.mLensIntentType;
            return lensQueryResult;
        }
    }

    public boolean getIsShoppyIntent() {
        return mIsShoppyIntent;
    }

    /*
     * Returns whether the Prime API specified a translate intent.
     */
    public boolean getIsTranslateIntent() {
        return mIsTranslateIntent;
    }

    public int getLensIntentType() {
        return mLensIntentType;
    }

    @Override
    public boolean equals(Object o) {
        if (o == null) {
            return false;
        }
        if (o == this) {
            return true;
        }

        if (!(o instanceof LensQueryResult)) {
            return false;
        }

        final LensQueryResult other = (LensQueryResult) o;

        return mLensIntentType == other.getLensIntentType()
                && mIsShoppyIntent == other.getIsShoppyIntent()
                && mIsTranslateIntent == other.getIsTranslateIntent();
    }
}
