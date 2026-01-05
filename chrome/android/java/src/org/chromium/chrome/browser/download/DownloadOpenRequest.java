// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.content.Context;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.OtrProfileId;

/** Parameters required to open a downloaded file. */
@NullMarked
public final class DownloadOpenRequest {
    final String mFilePath;
    @Nullable String mMimeType;
    @Nullable String mDownloadGuid;
    @Nullable OtrProfileId mOtrProfileId;
    @Nullable String mOriginalUrl;
    @Nullable String mReferrer;
    @DownloadOpenSource int mSource;
    final Context mContext;
    @Nullable String mFileName;

    private DownloadOpenRequest(Builder b) {
        this.mFilePath = b.mFilePath;
        this.mMimeType = b.mMimeType;
        this.mDownloadGuid = b.mDownloadGuid;
        this.mOtrProfileId = b.mOtrProfileId;
        this.mOriginalUrl = b.mOriginalUrl;
        this.mReferrer = b.mReferrer;
        this.mSource = b.mSource;
        this.mContext = b.mContext;
        this.mFileName = b.mFileName;
    }

    public static Builder builder(Context context, String filePath) {
        return new Builder(context, filePath);
    }

    public static final class Builder {
        private final String mFilePath;
        private final Context mContext;
        private @Nullable String mMimeType;
        private @Nullable String mDownloadGuid;
        private @Nullable OtrProfileId mOtrProfileId;
        private @Nullable String mOriginalUrl;
        private @Nullable String mReferrer;
        private @DownloadOpenSource int mSource;
        private @Nullable String mFileName;

        public Builder(Context context, String filePath) {
            this.mContext = context;
            this.mFilePath = filePath;
        }

        public Builder mimeType(@Nullable String v) {
            this.mMimeType = v;
            return this;
        }

        public Builder downloadGuid(@Nullable String v) {
            this.mDownloadGuid = v;
            return this;
        }

        public Builder otrProfileId(@Nullable OtrProfileId v) {
            this.mOtrProfileId = v;
            return this;
        }

        public Builder originalUrl(@Nullable String v) {
            this.mOriginalUrl = v;
            return this;
        }

        public Builder referrer(@Nullable String v) {
            this.mReferrer = v;
            return this;
        }

        public Builder source(@DownloadOpenSource int v) {
            this.mSource = v;
            return this;
        }

        public Builder fileName(@Nullable String v) {
            this.mFileName = v;
            return this;
        }

        public DownloadOpenRequest build() {
            return new DownloadOpenRequest(this);
        }
    }
}