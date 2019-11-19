// Copyright 2019 The Chromium Authors. All rights reserved.
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

    /**
     * @param username Username shown to the user.
     * @param password Password shown to the user.
     * @param originUrl Origin URL shown to the user in case this credential is a PSL match.
     * @param isPublicSuffixMatch Indicating whether the credential is a PSL match.
     */
    public Credential(String username, String password, String formattedUsername, String originUrl,
            boolean isPublicSuffixMatch) {
        assert originUrl != null : "Credential origin is null! Pass an empty one instead.";
        mUsername = username;
        mPassword = password;
        mFormattedUsername = formattedUsername;
        mOriginUrl = originUrl;
        mIsPublicSuffixMatch = isPublicSuffixMatch;
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
}
