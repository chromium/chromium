// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import android.accounts.Account;
import android.support.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.metrics.UmaSessionStats;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.sync.ModelType;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.base.PageTransition;

/**
 * Tests for UKM Sync integration.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
// Note we do not use the 'force-enable-metrics-reporting' flag for these tests as they would
// ignore the Sync setting we are verifying.

public class UkmTest {
    @Rule
    public SyncTestRule mSyncTestRule = new SyncTestRule();

    private static final String DEBUG_PAGE = "chrome://ukm/";

    @Before
    public void setUp() throws InterruptedException {
        mSyncTestRule.startMainActivityOnBlankPage();

        TestThreadUtils.runOnUiThreadBlocking(
                () -> UmaSessionStats.initMetricsAndCrashReportingForTesting());
    }

    @After
    public void tearDown() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> UmaSessionStats.unSetMetricsAndCrashReportingForTesting());
    }

    // TODO(rkaplow): Swap these methods with the JNI methods in UkmUtilsForTest.
    /*
     * These helper method should stay in sync with
     * chrome/browser/metrics/UkmTest.java.
     */
    public String getElementContent(Tab normalTab, String elementId) throws Exception {
        mSyncTestRule.loadUrlInTab(
                DEBUG_PAGE, PageTransition.TYPED | PageTransition.FROM_ADDRESS_BAR, normalTab);
        return JavaScriptUtils.executeJavaScriptAndWaitForResult(normalTab.getWebContents(),
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
    public void testMetricConsent() throws Exception {
        // Keep in sync with UkmBrowserTest.MetricsConsentCheck in
        // chrome/browser/metrics/ukm_browsertest.cc.
        // Make sure that UKM is disabled when metrics consent is revoked.

        TestThreadUtils.runOnUiThreadBlocking(
                () -> UmaSessionStats.updateMetricsAndCrashReportingForTesting(true));

        // Enable a Syncing account.
        Account account = mSyncTestRule.setUpTestAccountAndSignIn();
        Tab normalTab = mSyncTestRule.getActivity().getActivityTab();

        Assert.assertTrue("UKM Enabled:", isUkmEnabled(normalTab));

        String clientId = getUkmClientId(normalTab);

        // Ideally we should verify that unsent logs are cleared, which we do via HasUnsentUkmLogs()
        // in the ukm_browsertest.cc version.

        // Verify that after revoking consent, UKM is disabled.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> UmaSessionStats.updateMetricsAndCrashReportingForTesting(false));
        Assert.assertFalse("UKM Enabled:", isUkmEnabled(normalTab));

        // Re-enable consent, UKM is re-enabled.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> UmaSessionStats.updateMetricsAndCrashReportingForTesting(true));
        Assert.assertTrue("UKM Enabled:", isUkmEnabled(normalTab));

        // Client ID should have been reset.
        Assert.assertNotEquals("Client id:", clientId, getUkmClientId(normalTab));
    }

    @Test
    @SmallTest
    public void consentAddedButNoSyncCheck() throws Exception {
        // Keep in sync with UkmBrowserTest.ConsentAddedButNoSyncCheck in
        // chrome/browser/metrics/ukm_browsertest.cc.
        // Make sure that providing consent doesn't enable UKM when sync is disabled.

        TestThreadUtils.runOnUiThreadBlocking(
                () -> UmaSessionStats.updateMetricsAndCrashReportingForTesting(false));
        Tab normalTab = mSyncTestRule.getActivity().getActivityTab();
        Assert.assertFalse("UKM Enabled:", isUkmEnabled(normalTab));

        // Enable consent, Sync still not enabled so UKM should be disabled.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> UmaSessionStats.updateMetricsAndCrashReportingForTesting(true));
        Assert.assertFalse("UKM Enabled:", isUkmEnabled(normalTab));

        // Finally, sign in and UKM is enabled.
        Account account = mSyncTestRule.setUpTestAccountAndSignIn();
        Assert.assertTrue("UKM Enabled:", isUkmEnabled(normalTab));
    }

    @Test
    @SmallTest
    public void singleSyncSignoutCheck() throws Exception {
        // Keep in sync with UkmBrowserTest.SingleSyncSignoutCheck in
        // chrome/browser/metrics/ukm_browsertest.cc.
        // Make sure that UKM is disabled when an secondary passphrase is set.

        TestThreadUtils.runOnUiThreadBlocking(
                () -> UmaSessionStats.updateMetricsAndCrashReportingForTesting(true));

        // Enable a Syncing account.
        Account account = mSyncTestRule.setUpTestAccountAndSignIn();
        Tab normalTab = mSyncTestRule.getActivity().getActivityTab();
        Assert.assertTrue("UKM Enabled:", isUkmEnabled(normalTab));

        String clientId = getUkmClientId(normalTab);

        // Signing out should disable UKM.
        mSyncTestRule.signOut();

        Assert.assertFalse("UKM Enabled:", isUkmEnabled(normalTab));

        // Client ID should have been reset.
        Assert.assertNotEquals("Client id:", clientId, getUkmClientId(normalTab));
    }

    @Test
    @SmallTest
    public void singleDisableHistorySyncCheck() throws Exception {
        // Keep in sync with UkmBrowserTest.SingleDisableHistorySyncCheck in
        // chrome/browser/metrics/ukm_browsertest.cc.
        // Make sure that UKM is disabled when an secondary passphrase is set.

        TestThreadUtils.runOnUiThreadBlocking(
                () -> UmaSessionStats.updateMetricsAndCrashReportingForTesting(true));

        // Enable a Syncing account.
        Account account = mSyncTestRule.setUpTestAccountAndSignIn();
        Tab normalTab = mSyncTestRule.getActivity().getActivityTab();
        Assert.assertTrue("UKM Enabled:", isUkmEnabled(normalTab));

        String originalClientId = getUkmClientId(normalTab);

        // Disable Sync for history.
        mSyncTestRule.disableDataType(ModelType.TYPED_URLS);

        // Disable history sync does not disable UKM
        Assert.assertTrue("UKM Enabled:", isUkmEnabled(normalTab));
    }
}
