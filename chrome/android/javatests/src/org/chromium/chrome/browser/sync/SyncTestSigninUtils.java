// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.Promise;
import org.chromium.chrome.test.util.browser.signin.LiveSigninTestUtil;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.trusted_vault.SecurityDomainId;
import org.chromium.components.trusted_vault.TrustedVaultClient;
import org.chromium.google_apis.gaia.GaiaId;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/** Utility class for sign-in functionalities in native Sync browser tests. */
@JNINamespace("sync_test_utils_android")
final class SyncTestSigninUtils {
    private static class FakeTrustedVaultClientBackend extends TrustedVaultClient.EmptyBackend {
        private final Map<GaiaId, List<byte[]>> mKeys = new HashMap<>();

        @Override
        public Promise<List<byte[]>> fetchKeys(CoreAccountInfo accountInfo) {
            return Promise.fulfilled(
                    mKeys.getOrDefault(accountInfo.getGaiaId(), Collections.emptyList()));
        }

        @Override
        public void storeKeys(GaiaId gaiaId, List<byte[]> keys, int lastKeyVersion) {
            mKeys.put(gaiaId, new ArrayList<>(keys));
            TrustedVaultClient.get(SecurityDomainId.CHROME_SYNC).notifyKeysChanged(null);
        }
    }

    private static SigninTestRule sSigninTestRule;

    /** Sets up the test account and signs in. */
    // TODO(crbug.com/40066949): Remove param `withSyncConsent` once native tests for
    // sync-the-feature are removed.
    @CalledByNative
    private static void setUpAccountAndSignInForTesting(
            @JniType("AccountInfo") AccountInfo accountInfo, boolean withSyncConsent) {
        if (!withSyncConsent) {
            sSigninTestRule.addAccountThenSignin(accountInfo);
        } else {
            sSigninTestRule.addAccountThenSigninWithConsentLevelSync(accountInfo);
        }
    }

    /** Signs out from the current test account. */
    @CalledByNative
    private static void signOutForTesting() {
        sSigninTestRule.signOut();
    }

    /** Sets up the fake authentication environment. */
    @CalledByNative
    private static void setUpFakeAuthForTesting(boolean isNativeTest) {
        TrustedVaultClient.get(SecurityDomainId.CHROME_SYNC)
                .setBackendForTesting(new FakeTrustedVaultClientBackend());
        sSigninTestRule = new SigninTestRule(isNativeTest);
        sSigninTestRule.setUpRule();
    }

    /** Tears down the fake authentication environment. */
    @CalledByNative
    private static void tearDownFakeAuthForTesting() {
        // The seeded account is removed automatically when user signs out
        sSigninTestRule.tearDownRule();
        sSigninTestRule = null;
    }

    /** Add an account to the device and signs in for live testing. */
    // TODO(crbug.com/40066949): Remove param `withSyncConsent` once native tests for
    // sync-the-feature are removed.
    @CalledByNative
    private static void setUpLiveAccountAndSignInForTesting(
            @JniType("std::string") String accountName,
            @JniType("std::string") String password,
            boolean withSyncConsent) {
        if (!withSyncConsent) {
            LiveSigninTestUtil.getInstance()
                    .addAccountWithPasswordThenSignin(accountName, password);
        } else {
            LiveSigninTestUtil.getInstance()
                    .addAccountWithPasswordThenSigninWithConsentLevelSync(accountName, password);
        }
    }

    /**
     * Starts an asynchronous shutdown process for the live auth for tests. Needs to wait for
     * pending token requests to finish, so takes a callback to notify the native when its done.
     *
     * @param nativeCallback callback to be passed to `onShutdownComplete` after the shutdown is
     *     completed
     */
    @CalledByNative
    private static void shutdownLiveAuthForTesting(long callbackPtr) {
        AccountManagerFacade facade = AccountManagerFacadeProvider.getInstance();
        facade.disallowTokenRequestsForTesting();
        facade.waitForPendingTokenRequestsToComplete(
                () -> SyncTestSigninUtilsJni.get().onShutdownComplete(callbackPtr));
    }

    @NativeMethods
    interface Natives {
        /**
         * To be invoked after the shutdown initiated by `shutdownLiveAuthForTesting` is completed.
         *
         * @param nativeCallback the callback received by `shutdownLiveAuthForTesting`.
         */
        void onShutdownComplete(long callbackPtr);
    }
}
