// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;

/**
 * Utility class for sign-in functionalities in native Sync browser tests.
 */
final class SyncTestSigninUtils {
    private static final AccountManagerTestRule sAccountManagerTestRule =
            new AccountManagerTestRule();

    /**
     * Sets up the test account and signs in.
     */
    @CalledByNative
    private static void setUpAccountAndSignInForTesting() {
        sAccountManagerTestRule.waitForSeeding();
        sAccountManagerTestRule.addTestAccountThenSigninAndEnableSync();
    }

    /**
     * Sets up the test authentication environment.
     */
    @CalledByNative
    private static void setUpAuthForTesting() {
        sAccountManagerTestRule.setUpRule();
    }

    /**
     * Tears down the test authentication environment.
     */
    @CalledByNative
    private static void tearDownAuthForTesting() {
        // The seeded account is removed automatically when user signs out
        sAccountManagerTestRule.tearDownRule();
    }
}
