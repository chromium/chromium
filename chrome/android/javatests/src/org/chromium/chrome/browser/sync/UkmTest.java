// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.metrics.UmaSessionStats;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.ui.base.PageTransition;

/** Tests for UKM Sync integration. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
// Note we do not use the 'force-enable-metrics-reporting' flag for these tests as they would
// ignore the Sync setting we are verifying.

public class UkmTest {
    @Rule public SyncTestRule mSyncTestRule = new SyncTestRule();

    private static final String DEBUG_PAGE = "chrome://ukm/";

    @Before
    public void setUp() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> UmaSessionStats.initMetricsAndCrashReportingForTesting());
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> UmaSessionStats.unSetMetricsAndCrashReportingForTesting());
    }

    public void enableUkmUi(Tab normalTab) throws Exception {
        mSyncTestRule.loadUrlInTab(
                "chrome://chrome-urls/",
                PageTransition.TYPED | PageTransition.FROM_ADDRESS_BAR,
                normalTab);
        JavaScriptUtils.executeJavaScriptAndWaitForResult(
                normalTab.getWebContents(),
                "const app = document.body.querySelector('chrome-urls-app');"
                        + "const btn = app.shadowRoot.querySelector('cr-button');"
                        + "if (btn.textContent.includes('Enable')) {"
                        + "  btn.click();"
                        + "}");
    }

    public String getElementContent(Tab normalTab, String elementId) throws Exception {
        mSyncTestRule.loadUrlInTab(
                DEBUG_PAGE, PageTransition.TYPED | PageTransition.FROM_ADDRESS_BAR, normalTab);
        return JavaScriptUtils.executeJavaScriptAndWaitForResult(
                normalTab.getWebContents(),
                "document.getElementById('" + elementId + "').textContent");
    }

    public boolean isUkmEnabled(Tab normalTab) throws Exception {
        String state = getElementContent(normalTab, "state");
        Assert.assertTrue(
                "UKM state: " + state, state.equals("\"ENABLED\"") || state.equals("\"DISABLED\""));
        return state.equals("\"ENABLED\"");
    }

    public String getUkmClientId(Tab normalTab) throws Exception {
        return getElementContent(normalTab, "clientid");
    }

    @Test
    @SmallTest
    // TODO(crbug.com/40117796): Enable the corrersponding C++ test and delete this
    // test.
    // LINT.IfChange(ConsentAddedButNoSyncCheck)
    public void consentAddedButNoSyncCheck() throws Exception {
        // Keep in sync with UkmBrowserTest.ConsentAddedButNoSyncCheck in
        // chrome/browser/metrics/ukm_browsertest.cc.
        // Make sure that providing consent doesn't enable UKM when sync is disabled.

        ThreadUtils.runOnUiThreadBlocking(
                () -> UmaSessionStats.updateMetricsAndCrashReportingForTesting(false));
        Tab normalTab = mSyncTestRule.getActivityTab();
        enableUkmUi(normalTab);
        Assert.assertFalse("UKM Enabled:", isUkmEnabled(normalTab));

        // Enable consent, Sync still not enabled so UKM should be disabled.
        ThreadUtils.runOnUiThreadBlocking(
                () -> UmaSessionStats.updateMetricsAndCrashReportingForTesting(true));
        Assert.assertFalse("UKM Enabled:", isUkmEnabled(normalTab));

        // Finally, sync and UKM is enabled.
        mSyncTestRule.setUpAccountAndEnableHistorySync();
        Assert.assertTrue("UKM Enabled:", isUkmEnabled(normalTab));
    }

    // LINT.ThenChange(/chrome/browser/metrics/ukm_browsertest.cc:ConsentAddedButNoSyncCheck)

    @Test
    @SmallTest
    @EnableFeatures("RestructureMetricsConsentSettings")
    public void testConsentMigrationOnStartup_SignedOut() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Profile profile = ProfileManager.getLastUsedRegularProfile();
                    boolean migrationDone =
                            UserPrefs.get(profile)
                                    .getBoolean(Pref.ADVANCED_REPORTING_PROFILE_MIGRATION_DONE);
                    Assert.assertTrue("Migration done:", migrationDone);

                    // By default in fresh profile, MSBB is disabled.
                    // So kAdvancedReportingEnabled should be migrated to false.
                    boolean advancedReportingEnabled =
                            UserPrefs.get(profile).getBoolean(Pref.ADVANCED_REPORTING_ENABLED);
                    Assert.assertFalse("Advanced reporting enabled:", advancedReportingEnabled);
                });

        Tab normalTab = mSyncTestRule.getActivityTab();
        enableUkmUi(normalTab);
        Assert.assertFalse("UKM Enabled:", isUkmEnabled(normalTab));
    }

    @Test
    @SmallTest
    @DisableFeatures("RestructureMetricsConsentSettings")
    public void testConsentMigrationRollback() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> UmaSessionStats.updateMetricsAndCrashReportingForTesting(true));

        // Sign in and set up sync and MSBB.
        mSyncTestRule.setUpAccountAndEnableHistorySync();
        Tab normalTab = mSyncTestRule.getActivityTab();
        enableUkmUi(normalTab);
        Assert.assertTrue("UKM Enabled:", isUkmEnabled(normalTab));

        // Simulate a rollback scenario: the migration had run in a previous session
        // (when the flag was enabled) and wrote kAdvancedReportingEnabled to false.
        // Now, we are starting a session with the flag disabled (rolled back).
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Profile profile = ProfileManager.getLastUsedRegularProfile();
                    UserPrefs.get(profile)
                            .setBoolean(Pref.ADVANCED_REPORTING_PROFILE_MIGRATION_DONE, true);
                    UserPrefs.get(profile).setBoolean(Pref.ADVANCED_REPORTING_ENABLED, false);
                });

        // Since the feature flag is DISABLED, the UKM recorder should ignore
        // advanced_reporting_enabled (which is false) and remain ENABLED (reverting to MSBB logic).
        Assert.assertTrue("UKM Enabled after rollback:", isUkmEnabled(normalTab));
    }

    @Test
    @SmallTest
    @DisableFeatures("RestructureMetricsConsentSettings")
    public void testConsentMigrationRollback_MsbbDisabled() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> UmaSessionStats.updateMetricsAndCrashReportingForTesting(true));

        // Sign in and set up sync.
        mSyncTestRule.setUpAccountAndEnableHistorySync();
        Tab normalTab = mSyncTestRule.getActivityTab();
        enableUkmUi(normalTab);
        Assert.assertTrue("UKM Enabled:", isUkmEnabled(normalTab));

        // Disable MSBB.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Profile profile = ProfileManager.getLastUsedRegularProfile();
                    UserPrefs.get(profile)
                            .setBoolean(Pref.URL_KEYED_ANONYMIZED_DATA_COLLECTION_ENABLED, false);
                });
        Assert.assertFalse("UKM Disabled after MSBB disabled:", isUkmEnabled(normalTab));

        // Simulate a rollback scenario: the migration had run in a previous session
        // (when the flag was enabled) and wrote kAdvancedReportingEnabled to true.
        // Now, we are starting a session with the flag disabled (rolled back).
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Profile profile = ProfileManager.getLastUsedRegularProfile();
                    UserPrefs.get(profile)
                            .setBoolean(Pref.ADVANCED_REPORTING_PROFILE_MIGRATION_DONE, true);
                    UserPrefs.get(profile).setBoolean(Pref.ADVANCED_REPORTING_ENABLED, true);
                });

        // Since the feature flag is DISABLED, the UKM recorder should ignore
        // advanced_reporting_enabled (which is true) and remain DISABLED (reverting to MSBB logic).
        Assert.assertFalse(
                "UKM Disabled after rollback even if advanced reporting is true:",
                isUkmEnabled(normalTab));
    }

    @Test
    @SmallTest
    // TODO(crbug.com/40117796): Enable the corrersponding C++ test and delete this
    // test.

    // LINT.IfChange(SingleSyncSignoutCheck)
    public void singleSyncSignoutCheck() throws Exception {
        // Make sure that UKM is disabled when an explicit passphrase is set.

        ThreadUtils.runOnUiThreadBlocking(
                () -> UmaSessionStats.updateMetricsAndCrashReportingForTesting(true));

        // Enable a Syncing account.
        mSyncTestRule.setUpAccountAndEnableHistorySync();
        Tab normalTab = mSyncTestRule.getActivityTab();
        enableUkmUi(normalTab);
        Assert.assertTrue("UKM Enabled:", isUkmEnabled(normalTab));

        String clientId = getUkmClientId(normalTab);

        // Signing out should disable UKM.
        mSyncTestRule.signOut();

        Assert.assertFalse("UKM Enabled:", isUkmEnabled(normalTab));

        // Client ID should have been reset.
        Assert.assertNotEquals("Client id:", clientId, getUkmClientId(normalTab));
    }
    // LINT.ThenChange(/chrome/browser/metrics/ukm_browsertest.cc:SingleSyncSignoutCheck)
}
