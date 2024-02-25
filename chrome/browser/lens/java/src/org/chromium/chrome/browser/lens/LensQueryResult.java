// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.lens;

import android.text.TextUtils;

/** A wrapper class for the Lens image query result from Lens Prime SDK. */
public class LensQueryResult {
    private boolean mIsShoppyIntent;
    private boolean mIsTranslateIntent;
    private int mLensIntentType;
    private String mSessionId;
    private int mQueryId;

    /** Builder class for LensQueryParams. */
    public static class Builder {
        private boolean mIsShoppyIntent;
        private boolean mIsTranslateIntent;
        private int mLensIntentType;
        private String mSessionId;
        private int mQueryId;

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

        public Builder withSessionId(String sessionId) {
            this.mSessionId = sessionId;
            return this;
        }

        public Builder withQueryId(int queryId) {
            this.mQueryId = queryId;
            return this;
        }

        public LensQueryResult build() {
            LensQueryResult lensQueryResult = new LensQueryResult();
            lensQueryResult.mIsShoppyIntent = this.mIsShoppyIntent;
            lensQueryResult.mIsTranslateIntent = this.mIsTranslateIntent;
            lensQueryResult.mLensIntentType = this.mLensIntentType;
            lensQueryResult.mSessionId = this.mSessionId;
            lensQueryResult.mQueryId = this.mQueryId;
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

    public String getSessionId() {
        return mSessionId;
    }

    public int getQueryId() {
        return mQueryId;
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
                && mIsTranslateIntent == other.getIsTranslateIntent()
                // Return true for null values or equal values.
                && TextUtils.equals(mSessionId, other.getSessionId())
                && mQueryId == other.getQueryId();
    }
}
