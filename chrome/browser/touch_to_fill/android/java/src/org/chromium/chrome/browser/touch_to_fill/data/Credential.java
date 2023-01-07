// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.data;

import org.chromium.base.annotations.CalledByNative;

/**
 * This class holds the data used to represent a selectable credential in the Touch To Fill sheet.
 */
public class Credential {
    private final String mUsername;
    private final String mPassword;
    private final String mFormattedUsername;
    private final String mOriginUrl;
    private final boolean mIsPublicSuffixMatch;
    private final boolean mIsAffiliationBasedMatch;
    private final long mLastUsedMsSinceEpoch;

    /**
     * @param username Username shown to the user.
     * @param password Password shown to the user.
     * @param originUrl Origin URL shown to the user in case this credential is a PSL match.
     * @param isPublicSuffixMatch Indicating whether the credential is a PSL match.
     * @param isAffiliationBasedMatch Indicating whether the credential is an affiliation based
     * match (i.e. whether it is an Android credential).
     * @param lastUsedMsSinceEpoch Elapsed number of milliseconds from the unix epoch when the
     * credential was used the last time.
     */
    public Credential(String username, String password, String formattedUsername, String originUrl,
            boolean isPublicSuffixMatch, boolean isAffiliationBasedMatch,
            long lastUsedMsSinceEpoch) {
        assert originUrl != null : "Credential origin is null! Pass an empty one instead.";
        mUsername = username;
        mPassword = password;
        mFormattedUsername = formattedUsername;
        mOriginUrl = originUrl;
        mIsPublicSuffixMatch = isPublicSuffixMatch;
        mIsAffiliationBasedMatch = isAffiliationBasedMatch;
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
    public boolean isPublicSuffixMatch() {
        return mIsPublicSuffixMatch;
    }

    @CalledByNative
    public boolean isAffiliationBasedMatch() {
        return mIsAffiliationBasedMatch;
    }

    @CalledByNative
    public long lastUsedMsSinceEpoch() {
        return mLastUsedMsSinceEpoch;
    }

    public boolean isExactMatch() {
        return !mIsPublicSuffixMatch && !mIsAffiliationBasedMatch;
    }
}
