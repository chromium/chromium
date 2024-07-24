// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.all_passwords_bottom_sheet;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;

/**
 * This class holds the data used to represent a selectable credential in the
 * AllPasswordsBottomSheet.
 */
class Credential {
    private final String mUsername;
    private final String mPassword;
    private final String mFormattedUsername;
    private final String mOriginUrl;
    private final boolean mIsAndroidCredential;
    private final String mAppDisplayName;
    private final boolean mIsPlusAddressUsername;

    /**
     * @param username Username shown to the user.
     * @param password Password shown to the user.
     * @param originUrl Origin URL shown to the user in case this credential is a PSL match.
     * @param isAndroidCredential Indicating whether it is an Android credential.
     * @param appDisplayName The display name (e.g. Play Store name) of the Android application if
     *     it is an Android credential or app package name if app name is not available.
     * @param isPlusAddressUsername Indicating whether an existing plus address was used as
     *     username.
     */
    @CalledByNative
    Credential(
            @JniType("std::u16string") String username,
            @JniType("std::u16string") String password,
            @JniType("std::u16string") String formattedUsername,
            @JniType("std::string") String originUrl,
            boolean isAndroidCredential,
            @JniType("std::string") String appDisplayName,
            boolean isPlusAddressUsername) {
        assert originUrl != null : "Credential origin is null! Pass an empty one instead.";
        mUsername = username;
        mPassword = password;
        mFormattedUsername = formattedUsername;
        mOriginUrl = originUrl;
        mIsAndroidCredential = isAndroidCredential;
        mAppDisplayName = appDisplayName;
        mIsPlusAddressUsername = isPlusAddressUsername;
    }

    String getUsername() {
        return mUsername;
    }

    String getPassword() {
        return mPassword;
    }

    String getFormattedUsername() {
        return mFormattedUsername;
    }

    String getOriginUrl() {
        return mOriginUrl;
    }

    boolean isAndroidCredential() {
        return mIsAndroidCredential;
    }

    String getAppDisplayName() {
        return mAppDisplayName;
    }

    boolean isPlusAddressUsername() {
        return mIsPlusAddressUsername;
    }
}
