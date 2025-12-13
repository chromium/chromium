// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.data;

import org.jni_zero.CalledByNative;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.password_manager.GetLoginMatchType;
import org.chromium.url.GURL;

/**
 * This class holds the data used to represent a selectable credential in the Touch To Fill sheet.
 */
@NullMarked
public class Credential implements CredentialBase {
    private final String mUsername;
    private final String mPassword;
    private final String mFormattedUsername;
    private final String mOriginUrl;
    private final String mDisplayName;
    private final @GetLoginMatchType int mMatchType;
    private final long mLastUsedMsSinceEpoch;
    private final boolean mIsShared;
    private final String mSenderName;
    private final GURL mSenderProfileImageUrl;
    private final boolean mSharingNotificationDisplayed;
    private final boolean mIsBackupCredential;

    /** Helper class for building the Credential object. */
    public static class Builder {
        private String mUsername = "";
        private String mPassword = "";
        private String mFormattedUsername = "";
        private String mOriginUrl = "";
        private String mDisplayName = "";
        private @GetLoginMatchType int mMatchType;
        private long mLastUsedMsSinceEpoch;
        private boolean mIsShared;
        private String mSenderName = "";
        private GURL mSenderProfileImageUrl = GURL.emptyGURL();
        private boolean mSharingNotificationDisplayed;
        private boolean mIsBackupCredential;

        public Builder setUsername(String username) {
            mUsername = username;
            return this;
        }

        public Builder setPassword(String password) {
            mPassword = password;
            return this;
        }

        public Builder setFormattedUsername(String formattedUsername) {
            mFormattedUsername = formattedUsername;
            return this;
        }

        public Builder setOriginUrl(String originUrl) {
            mOriginUrl = originUrl;
            return this;
        }

        public Builder setDisplayName(String displayName) {
            mDisplayName = displayName;
            return this;
        }

        public Builder setMatchType(@GetLoginMatchType int matchType) {
            mMatchType = matchType;
            return this;
        }

        public Builder setLastUsedMsSinceEpoch(long lastUsedMsSinceEpoch) {
            mLastUsedMsSinceEpoch = lastUsedMsSinceEpoch;
            return this;
        }

        public Builder setIsShared(boolean isShared) {
            mIsShared = isShared;
            return this;
        }

        public Builder setSenderName(String senderName) {
            mSenderName = senderName;
            return this;
        }

        public Builder setSenderProfileImageUrl(GURL senderProfileImageUrl) {
            mSenderProfileImageUrl = senderProfileImageUrl;
            return this;
        }

        public Builder setSharingNotificationDisplayed(boolean sharingNotificationDisplayed) {
            mSharingNotificationDisplayed = sharingNotificationDisplayed;
            return this;
        }

        public Builder setIsBackupCredential(boolean isBackupCredential) {
            mIsBackupCredential = isBackupCredential;
            return this;
        }

        public Credential build() {
            return new Credential(this);
        }
    }

    private Credential(Builder builder) {
        assert builder.mOriginUrl != null : "Credential origin is null! Pass an empty one instead.";
        mUsername = builder.mUsername;
        mPassword = builder.mPassword;
        mFormattedUsername = builder.mFormattedUsername;
        mOriginUrl = builder.mOriginUrl;
        mDisplayName = builder.mDisplayName;
        mMatchType = builder.mMatchType;
        mLastUsedMsSinceEpoch = builder.mLastUsedMsSinceEpoch;
        mIsShared = builder.mIsShared;
        mSenderName = builder.mSenderName;
        mSenderProfileImageUrl = builder.mSenderProfileImageUrl;
        mSharingNotificationDisplayed = builder.mSharingNotificationDisplayed;
        mIsBackupCredential = builder.mIsBackupCredential;
    }

    @CalledByNative
    public String getUsername() {
        return mUsername;
    }

    @CalledByNative
    public String getPassword() {
        return mPassword;
    }

    public String getFormattedUsername() {
        return mFormattedUsername;
    }

    @CalledByNative
    public String getOriginUrl() {
        return mOriginUrl;
    }

    @CalledByNative
    public int getMatchType() {
        return mMatchType;
    }

    @CalledByNative
    public long lastUsedMsSinceEpoch() {
        return mLastUsedMsSinceEpoch;
    }

    @CalledByNative
    public String getDisplayName() {
        return mDisplayName;
    }

    public boolean isExactMatch() {
        return mMatchType == GetLoginMatchType.EXACT;
    }

    public boolean isShared() {
        return mIsShared;
    }

    public String getSenderName() {
        return mSenderName;
    }

    public GURL getSenderProfileImageUrl() {
        return mSenderProfileImageUrl;
    }

    public boolean isSharingNotificationDisplayed() {
        return mSharingNotificationDisplayed;
    }

    @CalledByNative
    public boolean isBackupCredential() {
        return mIsBackupCredential;
    }
}
