// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.waitUntilViewMatchesCondition;

import androidx.annotation.Nullable;

import org.chromium.base.Log;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.browsing_data.BrowsingDataBridge;
import org.chromium.chrome.browser.password_manager.PasswordStoreBridge;
import org.chromium.chrome.browser.password_manager.PasswordStoreCredential;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.url.GURL;

import java.util.concurrent.TimeoutException;

/**
 * Contains utilities for password change integration tests.
 */
public final class PasswordChangeFixtureTestUtils {
    /** The default maximum time to wait for a criteria to become valid. */
    public static final long MAX_WAIT_TIME_IN_MS = 60000;
    /** Default tag to log password change integration tests information. */
    public static final String TAG = "PasswordChangeTest";

    /**
     * Checks if two credentials are equal except for their password.
     *
     * @return True if only the credentials password differ, false otherwise.
     */
    public static boolean checkCredentialsDifferByPassword(
            PasswordStoreCredential credential1, PasswordStoreCredential credential2) {
        if (credential1 == null || credential2 == null) {
            return false;
        }

        return credential1.getUrl().equals(credential2.getUrl())
                && credential1.getUsername().equals(credential2.getUsername())
                && !credential1.getPassword().equals(credential2.getPassword());
    }

    /**
     * Search for a credential matching domain and username from a collection.
     *
     * @param domain Credential domain.
     * @param username Credential username.
     * @return Credential matching domain and username, null otherwise.
     */
    public static @Nullable PasswordStoreCredential getCredentialForDomainAndUser(
            PasswordStoreCredential[] credentials, GURL domain, String username) {
        for (int i = 0; i < credentials.length; i++) {
            PasswordStoreCredential credential = credentials[i];
            if (credential.getUrl().equals(domain) && credential.getUsername().equals(username)) {
                return credential;
            }
        }
        return null;
    }

    /**
     * Clears the browser's data for a certain time period.
     *
     * @param dataTypes List of BrowsingDataType elements to remove.
     * @param timePeriod Time period range for data removal.
     * @throws TimeoutException
     */
    public static void clearBrowsingData(int[] dataTypes, int timePeriod) throws TimeoutException {
        CallbackHelper helper = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            BrowsingDataBridge.getInstance().clearBrowsingData(
                    helper::notifyCalled, dataTypes, timePeriod);
        });
        helper.waitForCallback(0);
    }

    /**
     * Validates full password change run UI flow. Accepts generated password.
     */
    public static void validateFullRun() {
        // Opening site settings.
        waitUntilViewMatchesCondition(
                withText("Opening site settings..."), isDisplayed(), MAX_WAIT_TIME_IN_MS);

        // Filling out old password.
        waitUntilViewMatchesCondition(
                withText("Changing password..."), isDisplayed(), MAX_WAIT_TIME_IN_MS);

        // Requesting authorization to change the password.
        waitUntilViewMatchesCondition(
                withText("Use suggested password?"), isDisplayed(), MAX_WAIT_TIME_IN_MS);

        // Accept generated password.
        onView(withText("Use password")).perform(click());

        // Password was changed successfully.
        waitUntilViewMatchesCondition(
                withText("Changed password successfully"), isDisplayed(), MAX_WAIT_TIME_IN_MS);
    }

    /**
     * Logs all credentials in the password store.
     *
     * @param store Password store to log.
     */
    public static void logPasswordStoreCredentials(PasswordStoreBridge store) {
        logPasswordStoreCredentials(store, "");
    }

    /**
     * Logs all credentials in the password store.
     *
     * @param store Password store to log.
     * @param header Log header.
     */
    public static void logPasswordStoreCredentials(PasswordStoreBridge store, String header) {
        int numOfCredentials = store.getPasswordStoreCredentialsCount();
        StringBuilder sb = new StringBuilder();
        if (!header.isEmpty()) {
            sb.append("[" + header + "] ");
        }
        sb.append("Number of stored credentials: ").append(numOfCredentials).append(". ");
        sb.append("Credentials: ");
        for (PasswordStoreCredential credential : store.getAllCredentials()) {
            sb.append(credential).append(", ");
        }
        Log.i(TAG, sb.toString());
    }
}
