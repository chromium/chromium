// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.metrics;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.browsing_data.BrowsingDataType;
import org.chromium.chrome.browser.browsing_data.TimePeriod;
import org.chromium.chrome.browser.metrics.util.UkmUtilsForTest;
import org.chromium.chrome.browser.preferences.privacy.BrowsingDataBridge;
import org.chromium.chrome.browser.preferences.privacy.BrowsingDataBridge.OnClearBrowsingDataListener;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.components.metrics.MetricsSwitches;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Android UKM tests.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.
Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, MetricsSwitches.FORCE_ENABLE_METRICS_REPORTING})
public class UkmTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Before
    public void setUp() throws InterruptedException {
        mActivityTestRule.startMainActivityOnBlankPage();
    }

    /**
     * Closes the current tab.
     * @param incognito Whether to close an incognito or non-incognito tab.
     */
    protected void closeCurrentTab(final boolean incognito) {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mActivityTestRule.getActivity().getTabModelSelector().selectModel(incognito));
        ChromeTabUtils.closeCurrentTab(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());
    }

    protected void closeRegularTab() {
        closeCurrentTab(false);
    }

    protected void closeIncognitoTab() {
        closeCurrentTab(true);
    }

    // TODO(rkaplow): Simplify these by running then all in the UI thread via
    // @UIThreadTest.
    @Test
    @SmallTest
    public void testRegularPlusIncognitoCheck() throws Exception {
        // Keep in sync with UkmBrowserTest.RegularPlusIncognitoCheck in
        // chrome/browser/metrics/ukm_browsertest.cc.
        Tab normalTab = mActivityTestRule.getActivity().getActivityTab();

        TestThreadUtils.runOnUiThreadBlocking(
                () -> { Assert.assertTrue(UkmUtilsForTest.isEnabled()); });

        long originalClientId =
                TestThreadUtils
                        .runOnUiThreadBlocking(() -> { return UkmUtilsForTest.getClientId(); })
                        .longValue();
        Assert.assertFalse("Non-zero client id: " + originalClientId, originalClientId == 0);

        mActivityTestRule.newIncognitoTabFromMenu();

        TestThreadUtils.runOnUiThreadBlocking(
                () -> { Assert.assertFalse(UkmUtilsForTest.isEnabled()); });

        // Opening another regular tab mustn't enable UKM.
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { Assert.assertFalse(UkmUtilsForTest.isEnabled()); });

        // Opening and closing another Incognito tab mustn't enable UKM.
        mActivityTestRule.newIncognitoTabFromMenu();
        closeIncognitoTab();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { Assert.assertFalse(UkmUtilsForTest.isEnabled()); });

        closeRegularTab();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { Assert.assertFalse(UkmUtilsForTest.isEnabled()); });

        closeIncognitoTab();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { Assert.assertTrue(UkmUtilsForTest.isEnabled()); });

        // Client ID should not have been reset.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertEquals("Client id:", originalClientId, UkmUtilsForTest.getClientId());
        });
    }

    @Test
    @SmallTest
    public void testIncognitoPlusRegularCheck() {
        // Keep in sync with UkmBrowserTest.IncognitoPlusRegularCheck in
        // chrome/browser/metrics/ukm_browsertest.cc.

        // Start by closing all tabs.
        ChromeTabUtils.closeAllTabs(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());

        mActivityTestRule.newIncognitoTabFromMenu();

        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());
        Tab normalTab = mActivityTestRule.getActivity().getActivityTab();

        TestThreadUtils.runOnUiThreadBlocking(
                () -> { Assert.assertFalse(UkmUtilsForTest.isEnabled()); });

        closeIncognitoTab();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { Assert.assertTrue(UkmUtilsForTest.isEnabled()); });
    }

    @Test
    @SmallTest
    public void testHistoryDeleteCheck() throws Exception {
        // Keep in sync with UkmBrowserTest.HistoryDeleteCheck in
        // chrome/browser/metrics/ukm_browsertest.cc.

        // Start by closing all tabs.
        ChromeTabUtils.closeAllTabs(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());

        TestThreadUtils.runOnUiThreadBlocking(
                () -> { Assert.assertTrue(UkmUtilsForTest.isEnabled()); });

        long originalClientId =
                TestThreadUtils
                        .runOnUiThreadBlocking(() -> { return UkmUtilsForTest.getClientId(); })
                        .longValue();
        Assert.assertFalse("Non-zero client id: " + originalClientId, originalClientId == 0);

        // Record some dummy UKM data (adding a Source).
        final long sourceId = 0x54321;

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Write data under a dummy sourceId and verify it is there.
            UkmUtilsForTest.recordSourceWithId(sourceId);
            Assert.assertTrue(UkmUtilsForTest.hasSourceWithId(sourceId));
        });
        CallbackHelper callbackHelper = new CallbackHelper();

        // Clear all browsing history.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            BrowsingDataBridge.getInstance().clearBrowsingData(new OnClearBrowsingDataListener() {
                @Override
                public void onBrowsingDataCleared() {
                    callbackHelper.notifyCalled();
                }
            }, new int[] {BrowsingDataType.HISTORY}, TimePeriod.ALL_TIME);
        });
        callbackHelper.waitForCallback(0);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Verify that UKM is still running.
            Assert.assertTrue(UkmUtilsForTest.isEnabled());
            // The source under sourceId should be removed.
            Assert.assertFalse(UkmUtilsForTest.hasSourceWithId(sourceId));
            // Client ID should not have been reset.
            Assert.assertEquals("Client id:", originalClientId, UkmUtilsForTest.getClientId());
        });
    }
}
