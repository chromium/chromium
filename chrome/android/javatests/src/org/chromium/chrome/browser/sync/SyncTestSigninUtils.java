// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;

/**
 * Utility class for sign-in functionalities in native Sync browser tests.
 */
final class SyncTestSigninUtils {
    private static final SigninTestRule sSigninTestRule = new SigninTestRule();

    /**
     * Sets up the test account and signs in, but does not enable Sync.
     */
    @CalledByNative
    private static void setUpAccountAndSignInForTesting() {
        sSigninTestRule.addTestAccountThenSignin();
    }

    /**
     * Sets up the test account, signs in, and enables Sync-the-feature.
     */
    @CalledByNative
    private static void setUpAccountAndSignInAndEnableSyncForTesting() {
        sSigninTestRule.addTestAccountThenSigninAndEnableSync();
    }

    /**
     * Sets up the test authentication environment.
     */
    @CalledByNative
    private static void setUpAuthForTesting() {
        sSigninTestRule.setUpRule();
    }

    /**
     * Tears down the test authentication environment.
     */
    @CalledByNative
    private static void tearDownAuthForTesting() {
        // The seeded account is removed automatically when user signs out
        sSigninTestRule.tearDownRule();
    }
}
