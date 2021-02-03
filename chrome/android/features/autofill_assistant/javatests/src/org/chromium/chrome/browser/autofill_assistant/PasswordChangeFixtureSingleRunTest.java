// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.waitUntil;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.waitUntilViewMatchesCondition;
import static org.chromium.chrome.browser.autofill_assistant.PasswordChangeFixtureTestUtils.MAX_WAIT_TIME_IN_MS;
import static org.chromium.chrome.browser.autofill_assistant.PasswordChangeFixtureTestUtils.TAG;
import static org.chromium.chrome.browser.autofill_assistant.PasswordChangeFixtureTestUtils.checkCredentialsDifferByPassword;
import static org.chromium.chrome.browser.autofill_assistant.PasswordChangeFixtureTestUtils.clearBrowsingData;
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
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.url.GURL;

/**
 * Integration test for automated password change scripts.
 */
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@RunWith(ChromeJUnit4ClassRunner.class)
public class PasswordChangeFixtureSingleRunTest
        implements PasswordStoreBridge.PasswordStoreObserver {
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
     * Updated credential.
     */
    private PasswordStoreCredential mNewCredential;

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

        mTestRule.startCustomTabActivityWithIntent(CustomTabsTestUtils.createMinimalCustomTabIntent(
                InstrumentationRegistry.getTargetContext(),
                mTestRule.getTestServer().getURL(mParameters.getDomainUrl())));

        /**
         * PasswordStoreBridge requests credentials from the password store on initialization. The
         * request needs to be posted from the main thread.
         */
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mPasswordStoreBridge = new PasswordStoreBridge(this); });
    }

    @After
    public void tearDown() {
        mPasswordStoreBridge.clearAllPasswords();
        waitUntil(() -> mPasswordStoreBridge.getPasswordStoreCredentialsCount() == 0);
        TestThreadUtils.runOnUiThreadBlocking(() -> { mPasswordStoreBridge.destroy(); });
    }

    /**
     * Runs password change script on the provided domain for a particular user.
     */
    void runScriptForUser(String username) {
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> PasswordChangeLauncher.start(getWebContents().getTopLevelNativeWindow(),
                                getWebContents().getLastCommittedUrl(), username,
                                mParameters.getDebugBundleId(), mParameters.getDebugSocketId()));
    }

    /**
     * Checks the script successfully changes password in Chrome password manager. There should be
     * only one entry for that domain with the new password.
     */
    @Test
    @Manual
    public void testPasswordChangeFlow() throws Exception {
        PasswordStoreCredential credential =
                new PasswordStoreCredential(new GURL(mParameters.getDomainUrl()),
                        mParameters.getUsername(), mParameters.getPassword());

        // Insert credential into the password store.
        mPasswordStoreBridge.insertPasswordCredential(credential);

        // Wait until insert operation finishes.
        waitUntil(() -> mPasswordStoreBridge.getPasswordStoreCredentialsCount() == 1);

        // Run password change script for user.
        runScriptForUser(credential.getUsername());

        // Validate password change run. Update credential with generated password.
        validateFullRun();

        // Check credential has been updated and passwords differ.
        waitUntil(() -> checkCredentialsDifferByPassword(credential, mNewCredential));

        // Assert store contains a single credential.
        Assert.assertTrue("Store does not contain a single credential",
                mPasswordStoreBridge.getPasswordStoreCredentialsCount() == 1);

        logPasswordStoreCredentials(mPasswordStoreBridge, "Final password store state");
    }

    /**
     * Clear all browsing data (E.g Cookies, site settings, history). Checks the script
     * successfully changes password in Chrome password manager.
     */
    @Test
    @Manual
    public void testPasswordChangeNoCookies() throws Exception {
        clearBrowsingData(new int[] {BrowsingDataType.HISTORY, BrowsingDataType.CACHE,
                                  BrowsingDataType.COOKIES, BrowsingDataType.SITE_SETTINGS},
                TimePeriod.ALL_TIME);
        Log.i(TAG, "EVENT: Site settings cleared");

        PasswordStoreCredential credential =
                new PasswordStoreCredential(new GURL(mParameters.getDomainUrl()),
                        mParameters.getUsername(), mParameters.getPassword());

        // Insert credential into the password store.
        mPasswordStoreBridge.insertPasswordCredential(credential);

        // Wait until insert operation finishes.
        waitUntil(() -> mPasswordStoreBridge.getPasswordStoreCredentialsCount() == 1);

        // Run password change script for user.
        runScriptForUser(credential.getUsername());

        // Validate password change run. Update credential with generated password.
        validateFullRun();

        // Check credential has been updated and passwords differ.
        waitUntil(() -> checkCredentialsDifferByPassword(credential, mNewCredential));

        // Assert store contains a single credential.
        Assert.assertTrue("Store does not contain a single credential",
                mPasswordStoreBridge.getPasswordStoreCredentialsCount() == 1);

        logPasswordStoreCredentials(mPasswordStoreBridge, "Final password store state");
    }

    /**
     * Checks the script fails at login due to wrong credentials.
     */
    @Test
    @Manual
    public void testInvalidCredentials() throws Exception {
        PasswordStoreCredential credential =
                new PasswordStoreCredential(new GURL(mParameters.getDomainUrl()),
                        mParameters.getUsername(), mParameters.getPassword());

        // Insert credential into the password store.
        mPasswordStoreBridge.insertPasswordCredential(credential);

        // Wait until insert operation finishes.
        waitUntil(() -> mPasswordStoreBridge.getPasswordStoreCredentialsCount() == 1);

        // Run script.
        runScriptForUser(credential.getUsername());

        // Opening site settings.
        waitUntilViewMatchesCondition(
                withText("Opening site settings..."), isDisplayed(), MAX_WAIT_TIME_IN_MS);

        // Should fail during login. Wait for error opening site settings.
        waitUntilViewMatchesCondition(withText("Sorry, could not open site settings"),
                isDisplayed(), MAX_WAIT_TIME_IN_MS);

        // Assert password store contains only one credential.
        int countOfCredentials = mPasswordStoreBridge.getPasswordStoreCredentialsCount();
        Assert.assertTrue("Store does not contain a single credential", countOfCredentials == 1);

        // Assert initial credential has not changed.
        PasswordStoreCredential[] credentials = mPasswordStoreBridge.getAllCredentials();
        Assert.assertTrue("Initial credential was changed", credential.equals(credentials[0]));

        logPasswordStoreCredentials(mPasswordStoreBridge, "Final password store state");
    }

    @Override
    public void onSavedPasswordsChanged(int count) {
        logPasswordStoreCredentials(
                mPasswordStoreBridge, "EVENT: New set of credentials available");
        if (count == 1) {
            mNewCredential = mPasswordStoreBridge.getAllCredentials()[0];
        }
    }

    @Override
    public void onEdit(PasswordStoreCredential credential) {
        Log.i(TAG, "EVENT: Credential %s edited", credential.toString());
    }
}
