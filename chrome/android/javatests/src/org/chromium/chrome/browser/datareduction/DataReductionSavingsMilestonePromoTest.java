// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.datareduction;

import android.content.Context;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.AdvancedMockContext;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.net.spdyproxy.DataReductionProxySettings;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;

/**
 * Tests the DataReductionSavingsMilestonePromo. Tests that the promo data thresholds are properly
 * set from a field trial param.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class DataReductionSavingsMilestonePromoTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private static final int BYTES_IN_MB = 1024 * 1024;
    private static final int FIRST_PROMO_SIZE_MB = 100;
    private static final int SECOND_PROMO_SIZE_MB = 1024;
    private static final int COMMAND_LINE_FLAG_PROMO_SIZE_MB = 1;
    private static final String FIRST_PROMO_SIZE_STRING = "100 MB";
    private static final String SECOND_PROMO_SIZE_STRING = "1 GB";
    private static final String COMMAND_LINE_FLAG_PROMO_SIZE_STRING = "1 MB";

    private Context mContext;

    @Before
    public void setUp() throws InterruptedException, Throwable {
        // Using an AdvancedMockContext allows us to use a fresh in-memory SharedPreference.
        mContext = new AdvancedMockContext(InstrumentationRegistry.getInstrumentation()
                                                   .getTargetContext()
                                                   .getApplicationContext());
        mActivityTestRule.startMainActivityOnBlankPage();
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({"force-fieldtrials="
                    + DataReductionSavingsMilestonePromo.PROMO_FIELD_TRIAL_NAME + "/Enabled",
            "force-fieldtrial-params=" + DataReductionSavingsMilestonePromo.PROMO_FIELD_TRIAL_NAME
                    + ".Enabled:" + DataReductionSavingsMilestonePromo.PROMO_PARAM_NAME + "/"
                    + FIRST_PROMO_SIZE_MB + ";" + SECOND_PROMO_SIZE_MB})
    public void
    testDataReductionSavingsMilestonePromo() throws Throwable {
        mActivityTestRule.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                DataReductionProxySettings.getInstance().setDataReductionProxyEnabled(
                        mContext, true);
                Assert.assertFalse(
                        DataReductionPromoUtils.hasMilestonePromoBeenInitWithStartingSavedBytes());

                DataReductionSavingsMilestonePromo promo =
                        new DataReductionSavingsMilestonePromo(mActivityTestRule.getActivity(), 0);
                Assert.assertFalse(promo.shouldShowPromo());
                Assert.assertNull(promo.getPromoText());

                Assert.assertTrue(
                        DataReductionPromoUtils.hasMilestonePromoBeenInitWithStartingSavedBytes());
                Assert.assertEquals(
                        0, DataReductionPromoUtils.getDisplayedMilestonePromoSavedBytes());

                promo = new DataReductionSavingsMilestonePromo(
                        mActivityTestRule.getActivity(), FIRST_PROMO_SIZE_MB * BYTES_IN_MB);
                Assert.assertTrue(promo.shouldShowPromo());
                Assert.assertNotNull(promo.getPromoText());
                promo.onPromoTextSeen();
                Assert.assertEquals(FIRST_PROMO_SIZE_MB * BYTES_IN_MB,
                        DataReductionPromoUtils.getDisplayedMilestonePromoSavedBytes());

                promo = new DataReductionSavingsMilestonePromo(
                        mActivityTestRule.getActivity(), SECOND_PROMO_SIZE_MB * BYTES_IN_MB);
                Assert.assertTrue(promo.shouldShowPromo());
                Assert.assertNotNull(promo.getPromoText());
                promo.onPromoTextSeen();
                Assert.assertEquals(SECOND_PROMO_SIZE_MB * BYTES_IN_MB,
                        DataReductionPromoUtils.getDisplayedMilestonePromoSavedBytes());
            }
        });
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({"force-fieldtrials="
                    + DataReductionSavingsMilestonePromo.PROMO_FIELD_TRIAL_NAME + "/Enabled",
            "force-fieldtrial-params=" + DataReductionSavingsMilestonePromo.PROMO_FIELD_TRIAL_NAME
                    + ".Enabled:" + DataReductionSavingsMilestonePromo.PROMO_PARAM_NAME + "/"
                    + FIRST_PROMO_SIZE_MB + ";" + SECOND_PROMO_SIZE_MB})
    public void
    testDataReductionSavingsMilestonePromoExistingUser() throws Throwable {
        mActivityTestRule.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                DataReductionProxySettings.getInstance().setDataReductionProxyEnabled(
                        mContext, true);
                Assert.assertFalse(
                        DataReductionPromoUtils.hasMilestonePromoBeenInitWithStartingSavedBytes());

                DataReductionSavingsMilestonePromo promo = new DataReductionSavingsMilestonePromo(
                        mActivityTestRule.getActivity(), SECOND_PROMO_SIZE_MB * BYTES_IN_MB);
                Assert.assertFalse(promo.shouldShowPromo());
                Assert.assertNull(promo.getPromoText());
                Assert.assertEquals(SECOND_PROMO_SIZE_MB * BYTES_IN_MB,
                        DataReductionPromoUtils.getDisplayedMilestonePromoSavedBytes());
                Assert.assertTrue(
                        DataReductionPromoUtils.hasMilestonePromoBeenInitWithStartingSavedBytes());

                promo = new DataReductionSavingsMilestonePromo(
                        mActivityTestRule.getActivity(), SECOND_PROMO_SIZE_MB * BYTES_IN_MB + 1);
                Assert.assertFalse(promo.shouldShowPromo());
                Assert.assertNull(promo.getPromoText());
                Assert.assertEquals(SECOND_PROMO_SIZE_MB * BYTES_IN_MB,
                        DataReductionPromoUtils.getDisplayedMilestonePromoSavedBytes());
            }
        });
    }

    @Test
    @MediumTest
    @CommandLineFlags.
    Add({"enable-data-reduction-proxy-savings-promo", "disable-field-trial-config"})
    public void testDataReductionSavingsMilestonePromoCommandLineFlag() throws Throwable {
        mActivityTestRule.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                DataReductionProxySettings.getInstance().setDataReductionProxyEnabled(
                        mContext, true);
                Assert.assertFalse(
                        DataReductionPromoUtils.hasMilestonePromoBeenInitWithStartingSavedBytes());

                DataReductionSavingsMilestonePromo promo =
                        new DataReductionSavingsMilestonePromo(mActivityTestRule.getActivity(), 0);
                Assert.assertFalse(promo.shouldShowPromo());
                Assert.assertNull(promo.getPromoText());

                promo = new DataReductionSavingsMilestonePromo(mActivityTestRule.getActivity(),
                        COMMAND_LINE_FLAG_PROMO_SIZE_MB * BYTES_IN_MB);
                Assert.assertTrue(promo.shouldShowPromo());
                Assert.assertTrue(
                        promo.getPromoText().endsWith(COMMAND_LINE_FLAG_PROMO_SIZE_STRING));
                promo.onPromoTextSeen();
                Assert.assertEquals(COMMAND_LINE_FLAG_PROMO_SIZE_MB * BYTES_IN_MB,
                        DataReductionPromoUtils.getDisplayedMilestonePromoSavedBytes());

                promo = new DataReductionSavingsMilestonePromo(
                        mActivityTestRule.getActivity(), FIRST_PROMO_SIZE_MB * BYTES_IN_MB);
                Assert.assertFalse(promo.shouldShowPromo());
                Assert.assertNull(promo.getPromoText());
            }
        });
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({"enable-data-reduction-proxy-savings-promo",
            "force-fieldtrials=" + DataReductionSavingsMilestonePromo.PROMO_FIELD_TRIAL_NAME
                    + "/Enabled",
            "force-fieldtrial-params=" + DataReductionSavingsMilestonePromo.PROMO_FIELD_TRIAL_NAME
                    + ".Enabled:" + DataReductionSavingsMilestonePromo.PROMO_PARAM_NAME + "/"
                    + FIRST_PROMO_SIZE_MB + ";" + SECOND_PROMO_SIZE_MB})
    public void
    testDataReductionSavingsMilestonePromoCommandLineFlagWithFieldTrial() throws Throwable {
        mActivityTestRule.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                DataReductionProxySettings.getInstance().setDataReductionProxyEnabled(
                        mContext, true);
                Assert.assertFalse(
                        DataReductionPromoUtils.hasMilestonePromoBeenInitWithStartingSavedBytes());

                DataReductionSavingsMilestonePromo promo =
                        new DataReductionSavingsMilestonePromo(mActivityTestRule.getActivity(), 0);
                Assert.assertFalse(promo.shouldShowPromo());
                Assert.assertNull(promo.getPromoText());

                promo = new DataReductionSavingsMilestonePromo(mActivityTestRule.getActivity(),
                        COMMAND_LINE_FLAG_PROMO_SIZE_MB * BYTES_IN_MB);
                Assert.assertTrue(promo.shouldShowPromo());
                Assert.assertTrue(
                        promo.getPromoText().endsWith(COMMAND_LINE_FLAG_PROMO_SIZE_STRING));
                promo.onPromoTextSeen();
                Assert.assertEquals(COMMAND_LINE_FLAG_PROMO_SIZE_MB * BYTES_IN_MB,
                        DataReductionPromoUtils.getDisplayedMilestonePromoSavedBytes());

                promo = new DataReductionSavingsMilestonePromo(
                        mActivityTestRule.getActivity(), FIRST_PROMO_SIZE_MB * BYTES_IN_MB);
                Assert.assertTrue(promo.shouldShowPromo());
                Assert.assertTrue(promo.getPromoText().endsWith(FIRST_PROMO_SIZE_STRING));
                promo.onPromoTextSeen();
                Assert.assertEquals(FIRST_PROMO_SIZE_MB * BYTES_IN_MB,
                        DataReductionPromoUtils.getDisplayedMilestonePromoSavedBytes());
            }
        });
    }
}
