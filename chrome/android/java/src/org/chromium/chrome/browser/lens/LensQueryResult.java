// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.lens;

/**
 * A wrapper class for the Lens image query result from Lens Prime SDK.
 */
public class LensQueryResult {
    private boolean mIsShoppyIntent;
    private int mLensIntentType;

    /**
     * Builder class for LensQueryParams.
     */
    public static class Builder {
        private boolean mIsShoppyIntent;
        private int mLensIntentType;
        public Builder() {}

        public Builder withIsShoppyIntent(boolean isShoppyIntent) {
            this.mIsShoppyIntent = isShoppyIntent;
            return this;
        }

        public Builder withLensIntentType(int lensIntentType) {
            this.mLensIntentType = lensIntentType;
            return this;
        }

        public LensQueryResult build() {
            LensQueryResult lensQueryResult = new LensQueryResult();
            lensQueryResult.mIsShoppyIntent = this.mIsShoppyIntent;
            lensQueryResult.mLensIntentType = this.mLensIntentType;
            return lensQueryResult;
        }
    }

    public boolean getIsShoppyIntent() {
        return mIsShoppyIntent;
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
                && mIsShoppyIntent == other.getIsShoppyIntent();
    }
}