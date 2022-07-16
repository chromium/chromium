// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.incognito.reauth;

import android.os.Build;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.device_reauth.BiometricAuthRequester;
import org.chromium.chrome.browser.device_reauth.ReauthenticatorBridge;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

/**
 * This class is responsible for managing the Incognito re-authentication flow.
 */
public class IncognitoReauthManager {
    private static Boolean sIsIncognitoReauthFeatureAvailableForTesting;
    private ReauthenticatorBridge mReauthenticatorBridge;

    /**
     * A callback interface which is used for re-authentication in {@link
     * IncognitoReauthManager#startReauthenticationFlow(IncognitoReauthCallback)}.
     */
    public interface IncognitoReauthCallback {
        // This is invoked when either the Incognito re-authentication feature is not available or
        // the device screen lock is not setup or there's an authentication already in progress.
        void onIncognitoReauthNotPossible();
        // This is invoked when the Incognito re-authentication resulted in success.
        void onIncognitoReauthSuccess();
        // This is invoked when the Incognito re-authentication resulted in failure.
        void onIncognitoReauthFailure();
    }

    /**
     * Constructor for {@link IncognitoReauthManager}. Initialises |mReauthenticatorBridge|.
     */
    public IncognitoReauthManager() {
        mReauthenticatorBridge =
                new ReauthenticatorBridge(BiometricAuthRequester.INCOGNITO_REAUTH_PAGE);
    }

    @VisibleForTesting
    public IncognitoReauthManager(ReauthenticatorBridge reauthenticatorBridge) {
        mReauthenticatorBridge = reauthenticatorBridge;
    }

    /**
     * Starts the authentication flow. This is an asynchronous method call which would invoke the
     * passed {@link IncognitoReauthCallback} parameter once executed.
     *
     * @param incognitoReauthCallback A {@link IncognitoReauthCallback} callback that
     *         would be run once the authentication is executed.
     */
    public void startReauthenticationFlow(
            @NonNull IncognitoReauthCallback incognitoReauthCallback) {
        if (!mReauthenticatorBridge.canUseAuthentication()
                || !isIncognitoReauthFeatureAvailable()) {
            incognitoReauthCallback.onIncognitoReauthNotPossible();
            return;
        }

        mReauthenticatorBridge.reauthenticate(success -> {
            if (success) {
                incognitoReauthCallback.onIncognitoReauthSuccess();
            } else {
                incognitoReauthCallback.onIncognitoReauthFailure();
            }
        });
    }
    /**
     * @return A boolean indicating if the Incognito re-authentication feature is available.
     */
    public static boolean isIncognitoReauthFeatureAvailable() {
        if (sIsIncognitoReauthFeatureAvailableForTesting != null) {
            return sIsIncognitoReauthFeatureAvailableForTesting;
        }
        // The implementation relies on {@link BiometricManager} which was introduced in API
        // level 29. Android Q is not supported due to a potential bug in BiometricPrompt.
        return (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R)
                && ChromeFeatureList.isEnabled(
                        ChromeFeatureList.INCOGNITO_REAUTHENTICATION_FOR_ANDROID);
    }

    @VisibleForTesting
    public static void setIsIncognitoReauthFeatureAvailableForTesting(boolean isAvailable) {
        sIsIncognitoReauthFeatureAvailableForTesting = isAvailable;
    }
}
