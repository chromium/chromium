// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.data;

import org.jni_zero.CalledByNative;

import org.chromium.chrome.browser.password_manager.GetLoginMatchType;
import org.chromium.url.GURL;

/**
 * This class holds the data used to represent a selectable credential in the Touch To Fill sheet.
 */
public class Credential {
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

    /**
     * @param username Username shown to the user.
     * @param password Password shown to the user.
     * @param originUrl Origin URL used to obtain a favicon.
     * @param displayName App/Website name shown to the user in case this credential is not an exact
     *     match.
     * @param matchType Indicating which type of a match the credential.
     * @param lastUsedMsSinceEpoch Elapsed number of milliseconds from the unix epoch when the
     *     credential was used the last time.
     */
    public Credential(
            String username,
            String password,
            String formattedUsername,
            String originUrl,
            String displayName,
            @GetLoginMatchType int matchType,
            long lastUsedMsSinceEpoch) {
        this(
                username,
                password,
                formattedUsername,
                originUrl,
                displayName,
                matchType,
                lastUsedMsSinceEpoch,
                /* isShared */ false,
                /* senderName */ "",
                /* senderProfileImageUrl */ GURL.emptyGURL(),
                /* sharingNotificationDisplayed */ false);
    }

    /**
     * @param username Username shown to the user.
     * @param password Password shown to the user.
     * @param originUrl Origin URL used to obtain a favicon.
     * @param displayName App/Website name shown to the user in case this credential is not an exact
     *     match.
     * @param matchType Indicating which type of a match the credential.
     * @param lastUsedMsSinceEpoch Elapsed number of milliseconds from the unix epoch when the
     *     credential was used the last time.
     * @param isShared whether the password has been received via the password sharing feature.
     * @param senderName The name of the user who shared this password if it was shared.
     * @param senderProfileImageUrl Similar to senderName but for the avatar picture url.
     * @param sharingNotificationDisplayed Whether the user was notified about receiving this shared
     *     credential before.
     */
    public Credential(
            String username,
            String password,
            String formattedUsername,
            String originUrl,
            String displayName,
            @GetLoginMatchType int matchType,
            long lastUsedMsSinceEpoch,
            boolean isShared,
            String senderName,
            GURL senderProfileImageUrl,
            boolean sharingNotificationDisplayed) {
        assert originUrl != null : "Credential origin is null! Pass an empty one instead.";
        mUsername = username;
        mPassword = password;
        mFormattedUsername = formattedUsername;
        mOriginUrl = originUrl;
        mDisplayName = displayName;
        mMatchType = matchType;
        mLastUsedMsSinceEpoch = lastUsedMsSinceEpoch;
        mIsShared = isShared;
        mSenderName = senderName;
        mSenderProfileImageUrl = senderProfileImageUrl;
        mSharingNotificationDisplayed = sharingNotificationDisplayed;
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
}
