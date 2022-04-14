// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withContentDescription;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.allOf;

import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.waitUntil;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.waitUntilViewMatchesCondition;
import static org.chromium.chrome.browser.autofill_assistant.PasswordChangeFixtureTestUtils.MAX_WAIT_BETWEEN_TESTS_IN_MS;
import static org.chromium.chrome.browser.autofill_assistant.PasswordChangeFixtureTestUtils.MAX_WAIT_TIME_IN_MS;
import static org.chromium.chrome.browser.autofill_assistant.PasswordChangeFixtureTestUtils.TAG;
import static org.chromium.chrome.browser.autofill_assistant.PasswordChangeFixtureTestUtils.checkCredentialsDifferByPassword;
import static org.chromium.chrome.browser.autofill_assistant.PasswordChangeFixtureTestUtils.clearBrowsingData;
import static org.chromium.chrome.browser.autofill_assistant.PasswordChangeFixtureTestUtils.getCredentialForDomainAndUser;
import static org.chromium.chrome.browser.autofill_assistant.PasswordChangeFixtureTestUtils.logPasswordStoreCredentials;
import static org.chromium.chrome.browser.autofill_assistant.PasswordChangeFixtureTestUtils.validateFullRun;

import android.support.test.InstrumentationRegistry;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Log;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Manual;
import org.chromium.chrome.browser.browsing_data.BrowsingDataType;
import org.chromium.chrome.browser.browsing_data.TimePeriod;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.customtabs.CustomTabsTestUtils;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.password_manager.PasswordChangeLauncher;
import org.chromium.chrome.browser.password_manager.PasswordStoreBridge;
import org.chromium.chrome.browser.password_manager.PasswordStoreCredential;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.autofill_assistant.AutofillAssistantPreferencesUtil;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Integration test for automated password change scripts.
 */
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@RunWith(ChromeJUnit4ClassRunner.class)
public class PasswordChangeFixtureTest implements PasswordStoreBridge.PasswordStoreObserver {
    @Rule
    public CustomTabActivityTestRule mTestRule = new CustomTabActivityTestRule();

    /**
     * Test settings parameters.
     */
    private PasswordChangeFixtureParameters mParameters;

    /**
     * Password store bridge.
     */
    private PasswordStoreBridge mPasswordStoreBridge;

    /**
     * Cache of the most recently obtained saved credentials.
     */
    private PasswordStoreCredential[] mCredentials;

    private WebContents getWebContents() {
        return mTestRule.getWebContents();
    }

    private CustomTabActivity getActivity() {
        return mTestRule.getActivity();
    }

    @Before
    public void setUp() throws Exception {
        AutofillAssistantPreferencesUtil.setInitialPreferences(true);
        mParameters = PasswordChangeFixtureParameters.loadFromCommandLine();

        AutofillAssistantTestEndpointService testService =
                new AutofillAssistantTestEndpointService(mParameters.getAutofillAssistantUrl());
        testService.scheduleForInjection();

        Log.i(TAG, "[Test started]");

        mTestRule.startCustomTabActivityWithIntent(CustomTabsTestUtils.createMinimalCustomTabIntent(
                InstrumentationRegistry.getTargetContext(), mParameters.getDomainUrl().getSpec()));

        /**
         * PasswordStoreBridge requests credentials from the password store on initialization. The
         * request needs to be posted from the main thread.
         */
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mPasswordStoreBridge = new PasswordStoreBridge();
            mPasswordStoreBridge.addObserver(this, false);
            // Load initial credentials.
            PasswordStoreCredential[] seedCredentials = mParameters.getSeedCredentials();
            for (int i = 0; i < seedCredentials.length; i++) {
                mPasswordStoreBridge.insertPasswordCredential(seedCredentials[i]);
            }
        });

        // Wait until operation finishes and credentials cache is updated.
        waitUntil(() -> mCredentials.length == mParameters.getSeedCredentials().length);
    }

    @After
    public void tearDown() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mPasswordStoreBridge.removeObserver(this);
            mPasswordStoreBridge.destroy();
        });
    }

    /**
     * Runs password change script on the provided domain for a particular user.
     */
    void runScriptForUser(String username) {
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> PasswordChangeLauncher.start(getWebContents().getTopLevelNativeWindow(),
                                mParameters.getDomainUrl(), username, /*skipLogin=*/false,
                                mParameters.getDebugBundleId(), mParameters.getDebugSocketId()));
    }

    /**
     * Performs and validates a single password change script run.
     */
    private void testRun(String username) {
        // Fetch login credential.
        PasswordStoreCredential initialCredential =
                getCredentialForDomainAndUser(mCredentials, mParameters.getDomainUrl(), username);
        // Run password change script for user.
        runScriptForUser(username);

        // Validate password change run. Update credential with generated password.
        validateFullRun();

        // Check credential has been updated.
        waitUntil(() -> {
            PasswordStoreCredential updatedCredential = getCredentialForDomainAndUser(
                    mCredentials, mParameters.getDomainUrl(), username);
            return checkCredentialsDifferByPassword(initialCredential, updatedCredential);
        });
    }

    /**
     * Runs the script multiple times (defined by --num-runs) consecutively. Checks the script
     * successfully changes password in Chrome password manager. There should be only one entry for
     * that domain with the new password.
     */
    @Test
    @Manual
    public void testDefaultRuns() throws Exception {
        for (int i = 0; i < mParameters.getNumRuns(); i++) {
            // Run and test script.
            testRun(mParameters.getUsername());

            if (i + 1 < mParameters.getNumRuns()) {
                Log.i(TAG, "[Run #%s succeeded]", String.valueOf(i + 1));
                Thread.sleep(MAX_WAIT_BETWEEN_TESTS_IN_MS);
            }
        }

        logPasswordStoreCredentials(mPasswordStoreBridge, "Final password store state");
    }

    /**
     * Clear all browsing data (E.g Cookies, site settings, history). Checks the script
     * successfully changes password in Chrome password manager.
     */
    @Test
    @Manual
    public void testNoCookies() throws Exception {
        for (int i = 0; i < mParameters.getNumRuns(); i++) {
            clearBrowsingData(new int[] {BrowsingDataType.HISTORY, BrowsingDataType.CACHE,
                                      BrowsingDataType.COOKIES, BrowsingDataType.SITE_SETTINGS},
                    TimePeriod.ALL_TIME);
            Log.i(TAG, "[Site settings cleared]");

            // Run and test password change.
            testRun(mParameters.getUsername());

            if (i + 1 < mParameters.getNumRuns()) {
                Log.i(TAG, "[Run #%s succeeded]", String.valueOf(i + 1));
                Thread.sleep(MAX_WAIT_BETWEEN_TESTS_IN_MS);
            }
        }

        logPasswordStoreCredentials(mPasswordStoreBridge, "Final password store state");
    }

    /**
     * Checks the script fails at login due to wrong credentials.
     */
    @Test
    @Manual
    public void testInvalidCredentials() throws Exception {
        // Fetch login credential for username.
        PasswordStoreCredential initialCredential = getCredentialForDomainAndUser(
                mCredentials, mParameters.getDomainUrl(), mParameters.getUsername());
        // Run script.
        runScriptForUser(mParameters.getUsername());

        // Opening site's settings.
        waitUntilViewMatchesCondition(
                withText("Opening site's settings..."), isDisplayed(), MAX_WAIT_TIME_IN_MS);

        // Should fail during login. Wait for error opening site's settings.
        waitUntilViewMatchesCondition(
                withText("Can't change your password"), isDisplayed(), MAX_WAIT_TIME_IN_MS);

        // Assert initial credential has not changed.
        PasswordStoreCredential newCredential = getCredentialForDomainAndUser(
                mCredentials, mParameters.getDomainUrl(), mParameters.getUsername());
        Assert.assertTrue(
                "Initial credential was changed", initialCredential.equals(newCredential));

        logPasswordStoreCredentials(mPasswordStoreBridge, "Final password store state");
    }

    /**
     * Checks the script does not introduce unexpected changes if the generated password is
     * rejected.
     */
    @Test
    @Manual
    public void testUserDeclinesGeneratedPassword() throws Exception {
        // Fetch login credential for username.
        PasswordStoreCredential initialCredential = getCredentialForDomainAndUser(
                mCredentials, mParameters.getDomainUrl(), mParameters.getUsername());
        // Run script.
        runScriptForUser(mParameters.getUsername());

        // Opening site's settings.
        waitUntilViewMatchesCondition(
                withText("Opening site's settings..."), isDisplayed(), MAX_WAIT_TIME_IN_MS);

        // Filling out old password.
        waitUntilViewMatchesCondition(
                withText("Changing password..."), isDisplayed(), MAX_WAIT_TIME_IN_MS);

        // Requesting authorization to change the password.
        waitUntilViewMatchesCondition(
                withText("Use suggested password?"), isDisplayed(), MAX_WAIT_TIME_IN_MS);

        // Simulate the user declining the generated password.
        onView(allOf(withContentDescription("Close"), isDisplayed())).perform(click());

        // Assert initial credential has not changed.
        PasswordStoreCredential newCredential = getCredentialForDomainAndUser(
                mCredentials, mParameters.getDomainUrl(), mParameters.getUsername());
        Assert.assertTrue(
                "Initial credential was changed", initialCredential.equals(newCredential));

        logPasswordStoreCredentials(mPasswordStoreBridge, "Final password store state");
    }

    /**
     * Fills out the current password field with a wrong password, submits the
     * password change form and validates that the script ends gracefully with an error.
     */
    @Test
    @Manual
    public void testWrongCurrentPassword() throws Exception {
        // Fetch login credential for username.
        PasswordStoreCredential initialCredential = getCredentialForDomainAndUser(
                mCredentials, mParameters.getDomainUrl(), mParameters.getUsername());
        // Run script.
        runScriptForUser(mParameters.getUsername());

        // Opening site's settings.
        waitUntilViewMatchesCondition(
                withText("Opening site's settings..."), isDisplayed(), MAX_WAIT_TIME_IN_MS);

        // Filling out old password.
        waitUntilViewMatchesCondition(
                withText("Changing password..."), isDisplayed(), MAX_WAIT_TIME_IN_MS);

        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mPasswordStoreBridge.editPassword(initialCredential, "wrongpassword"); });

        // Requesting authorization to change the password.
        waitUntilViewMatchesCondition(
                withText("Use suggested password?"), isDisplayed(), MAX_WAIT_TIME_IN_MS);

        // Accept generated password.
        onView(withText("Use password")).perform(click());

        // Password change submission failed.
        waitUntilViewMatchesCondition(
                withText("Can't change your password"), isDisplayed(), MAX_WAIT_TIME_IN_MS);

        logPasswordStoreCredentials(mPasswordStoreBridge, "Final password store state");
    }

    /**
     * Runs the password change flow for all credentials in the store and validates the changes.
     */
    @Test
    @Manual
    public void testMultipleCredentials() throws Exception {
        for (int run = 0; run < mParameters.getNumRuns(); run++) {
            // Maintain a reference to the current set of credentials.
            PasswordStoreCredential[] initialCredentials = mCredentials;
            // Run password change for all credentials.
            for (int i = 0; i < initialCredentials.length; i++) {
                // Run and test script.
                testRun(initialCredentials[i].getUsername());
                if (i + 1 < initialCredentials.length) {
                    Thread.sleep(MAX_WAIT_BETWEEN_TESTS_IN_MS);
                }
            }

            if (run + 1 < mParameters.getNumRuns()) {
                Log.i(TAG, "[Run #%s succeeded]", String.valueOf(run + 1));
                Thread.sleep(MAX_WAIT_BETWEEN_TESTS_IN_MS);
            }
        }

        logPasswordStoreCredentials(mPasswordStoreBridge, "Final password store state");
    }

    @Override
    public void onSavedPasswordsChanged(int count) {
        logPasswordStoreCredentials(mPasswordStoreBridge, "New set of credentials available");
        mCredentials = mPasswordStoreBridge.getAllCredentials();
    }

    @Override
    public void onEdit(PasswordStoreCredential credential) {
        Log.i(TAG, "[Credential %s edited]", credential.toString());
    }
}
