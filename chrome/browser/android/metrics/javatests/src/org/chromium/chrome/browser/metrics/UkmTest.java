// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.metrics;

import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.browsing_data.BrowsingDataBridge;
import org.chromium.chrome.browser.browsing_data.BrowsingDataBridge.OnClearBrowsingDataListener;
import org.chromium.chrome.browser.browsing_data.BrowsingDataType;
import org.chromium.chrome.browser.browsing_data.TimePeriod;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.metrics.util.UkmUtilsForTest;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.signin.services.UnifiedConsentServiceBridge;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.components.metrics.MetricsSwitches;

/** Android UKM tests. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    MetricsSwitches.FORCE_ENABLE_METRICS_REPORTING
})
public class UkmTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Before
    public void setUp() throws InterruptedException {
        mActivityTestRule.startMainActivityOnBlankPage();
    }

    // TODO(crbug.com/40117796): Move this to ukm_browsertest.cc.
    @Test
    @SmallTest
    public void testHistoryDeleteCheck() throws Exception {
        // Keep in sync with UkmBrowserTest.HistoryDeleteCheck in
        // chrome/browser/metrics/ukm_browsertest.cc.

        // Start by closing all tabs.
        ChromeTabUtils.closeAllTabs(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Profile profile = ProfileManager.getLastUsedRegularProfile();
                    UnifiedConsentServiceBridge.setUrlKeyedAnonymizedDataCollectionEnabled(
                            profile, true);
                    Assert.assertTrue(UkmUtilsForTest.isEnabled());
                });

        long originalClientId =
                ThreadUtils.runOnUiThreadBlocking(
                                () -> {
                                    return UkmUtilsForTest.getClientId();
                                })
                        .longValue();
        Assert.assertFalse("Non-zero client id: " + originalClientId, originalClientId == 0);

        // Record some placeholder UKM data (adding a Source).
        final long sourceId = 0x54321;

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Write data under a placeholder sourceId and verify it is there.
                    UkmUtilsForTest.recordSourceWithId(sourceId);
                    Assert.assertTrue(UkmUtilsForTest.hasSourceWithId(sourceId));
                });
        CallbackHelper callbackHelper = new CallbackHelper();

        // Clear all browsing history.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    BrowsingDataBridge.getForProfile(ProfileManager.getLastUsedRegularProfile())
                            .clearBrowsingData(
                                    new OnClearBrowsingDataListener() {
                                        @Override
                                        public void onBrowsingDataCleared() {
                                            callbackHelper.notifyCalled();
                                        }
                                    },
                                    new int[] {BrowsingDataType.HISTORY},
                                    TimePeriod.ALL_TIME);
                });
        callbackHelper.waitForCallback(0);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Verify that UKM is still running.
                    Assert.assertTrue(UkmUtilsForTest.isEnabled());
                    // The source under sourceId should be removed.
                    Assert.assertFalse(UkmUtilsForTest.hasSourceWithId(sourceId));
                    // Client ID should not have been reset.
                    Assert.assertEquals(
                            "Client id:", originalClientId, UkmUtilsForTest.getClientId());
                });
    }
}
