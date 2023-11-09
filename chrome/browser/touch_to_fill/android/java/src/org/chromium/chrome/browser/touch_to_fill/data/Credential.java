// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.data;

import org.jni_zero.CalledByNative;

import org.chromium.chrome.browser.password_manager.GetLoginMatchType;

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

    /**
     * @param username Username shown to the user.
     * @param password Password shown to the user.
     * @param originUrl Origin URL used to obtain a favicon.
     * @param displayName App/Website name shown to the user in case this credential is not an exact
     *         match.
     * @param matchType Indicating which type of a match the credential.
     * @param lastUsedMsSinceEpoch Elapsed number of milliseconds from the unix epoch when the
     * credential was used the last time.
     */
    public Credential(String username, String password, String formattedUsername, String originUrl,
            String displayName, @GetLoginMatchType int matchType, long lastUsedMsSinceEpoch) {
        assert originUrl != null : "Credential origin is null! Pass an empty one instead.";
        mUsername = username;
        mPassword = password;
        mFormattedUsername = formattedUsername;
        mOriginUrl = originUrl;
        mDisplayName = displayName;
        mMatchType = matchType;
        mLastUsedMsSinceEpoch = lastUsedMsSinceEpoch;
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
}
