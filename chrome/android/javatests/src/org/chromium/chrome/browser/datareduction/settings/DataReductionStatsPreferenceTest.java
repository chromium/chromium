// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.datareduction.settings;

import static org.chromium.third_party.android.datausagechart.ChartDataUsageView.MAXIMUM_DAYS_IN_CHART;
import static org.chromium.third_party.android.datausagechart.ChartDataUsageView.MINIMUM_DAYS_IN_CHART;

import android.content.Context;
import android.support.test.InstrumentationRegistry;
import android.text.format.DateUtils;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.UiThreadTest;
import org.chromium.base.test.util.AdvancedMockContext;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.net.spdyproxy.DataReductionProxySettings;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.test.ChromeBrowserTestRule;

/**
 * Unit test suite for DataReductionStatsPreference.
 */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class DataReductionStatsPreferenceTest {
    @Rule
    public final ChromeBrowserTestRule mChromeBrowserTestRule = new ChromeBrowserTestRule();

    /**
     * Key used to save the date that the site breakdown should be shown. If the user has historical
     * data saver stats, the site breakdown cannot be shown for DAYS_IN_CHART.
     */
    private static final String PREF_DATA_REDUCTION_SITE_BREAKDOWN_ALLOWED_DATE =
            "data_reduction_site_breakdown_allowed_date";

    public static final int DAYS_IN_CHART = 30;

    private Context mContext;
    private TestDataReductionProxySettings mSettings;

    private static class TestDataReductionProxySettings extends DataReductionProxySettings {
        private long mLastUpdateInMillis;
        private long[] mReceivedNetworkStatsHistory;

        /**
         * Returns the time that the data reduction statistics were last updated.
         * @return The last update time in milliseconds since the epoch.
         */
        @Override
        public long getDataReductionLastUpdateTime() {
            return mLastUpdateInMillis;
        }

        /**
         * Sets the time that the data reduction statistics were last updated in milliseconds since
         * the epoch. This is only for testing and does not update the native pref values.
         */
        public void setDataReductionLastUpdateTime(long lastUpdateInMillis) {
            mLastUpdateInMillis = lastUpdateInMillis;
        }

        @Override
        public long[] getReceivedNetworkStatsHistory() {
            if (mReceivedNetworkStatsHistory == null) return new long[0];
            return mReceivedNetworkStatsHistory;
        }

        public void setReceivedNetworkStatsHistory(long[] receivedNetworkStatsHistory) {
            mReceivedNetworkStatsHistory = receivedNetworkStatsHistory;
        }
    }

    @Before
    public void setUp() {
        // Using an AdvancedMockContext allows us to use a fresh in-memory SharedPreference.
        mContext = new AdvancedMockContext(InstrumentationRegistry.getInstrumentation()
                                                   .getTargetContext()
                                                   .getApplicationContext());
        ContextUtils.initApplicationContextForTests(mContext);
        mSettings = new TestDataReductionProxySettings();
        DataReductionProxySettings.setInstanceForTesting(mSettings);
    }

    /**
     * Tests that the site breakdown pref is initialized to now if there aren't historical stats.
     */
    @Test
    @SmallTest
    @UiThreadTest
    @Feature({"DataReduction"})
    public void testInitializeSiteBreakdownPrefNow() {
        long beforeTime = System.currentTimeMillis();
        DataReductionStatsPreference.initializeDataReductionSiteBreakdownPref();
        long afterTime = System.currentTimeMillis();

        Assert.assertTrue(SharedPreferencesManager.getInstance().readLong(
                                  PREF_DATA_REDUCTION_SITE_BREAKDOWN_ALLOWED_DATE, -1)
                >= beforeTime);
        Assert.assertTrue(SharedPreferencesManager.getInstance().readLong(
                                  PREF_DATA_REDUCTION_SITE_BREAKDOWN_ALLOWED_DATE, -1)
                <= afterTime);

        // Tests that the site breakdown pref isn't initialized again if the pref was
        // already set.
        DataReductionStatsPreference.initializeDataReductionSiteBreakdownPref();

        // Pref should still be the same value as before.
        Assert.assertTrue(SharedPreferencesManager.getInstance().readLong(
                                  PREF_DATA_REDUCTION_SITE_BREAKDOWN_ALLOWED_DATE, -1)
                >= beforeTime);
        Assert.assertTrue(SharedPreferencesManager.getInstance().readLong(
                                  PREF_DATA_REDUCTION_SITE_BREAKDOWN_ALLOWED_DATE, -1)
                <= afterTime);
    }

    /**
     * Tests that the site breakdown pref is initialized to 30 from Data Saver's last update time if
     * there are historical stats.
     */
    @Test
    @SmallTest
    @UiThreadTest
    @Feature({"DataReduction"})
    public void testInitializeSiteBreakdownPrefHistoricalStats() {
        // Make the last update one day ago.
        long lastUpdateInDays = 1;
        mSettings.setDataReductionLastUpdateTime(
                System.currentTimeMillis() - lastUpdateInDays * DateUtils.DAY_IN_MILLIS);
        long lastUpdateInMillis = mSettings.getDataReductionLastUpdateTime();
        DataReductionStatsPreference.initializeDataReductionSiteBreakdownPref();

        Assert.assertEquals(lastUpdateInMillis + DAYS_IN_CHART * DateUtils.DAY_IN_MILLIS,
                SharedPreferencesManager.getInstance().readLong(
                        PREF_DATA_REDUCTION_SITE_BREAKDOWN_ALLOWED_DATE, -1));
    }

    /**
     * Tests that the site breakdown pref is initialized to now if there are historical stats, but
     * they are more than 30 days old.
     */
    @Test
    @SmallTest
    @UiThreadTest
    @Feature({"DataReduction"})
    public void testInitializeSiteBreakdownPrefOldHistoricalStats() {
        mSettings.setDataReductionLastUpdateTime(
                System.currentTimeMillis() - DAYS_IN_CHART * DateUtils.DAY_IN_MILLIS);
        long beforeTime = System.currentTimeMillis();
        DataReductionStatsPreference.initializeDataReductionSiteBreakdownPref();
        long afterTime = System.currentTimeMillis();

        Assert.assertTrue(SharedPreferencesManager.getInstance().readLong(
                                  PREF_DATA_REDUCTION_SITE_BREAKDOWN_ALLOWED_DATE, -1)
                >= beforeTime);
        Assert.assertTrue(SharedPreferencesManager.getInstance().readLong(
                                  PREF_DATA_REDUCTION_SITE_BREAKDOWN_ALLOWED_DATE, -1)
                <= afterTime);
    }

    /**
     * Tests the timespan of the usage graph when Data Saver was enabled today.
     */
    @Test
    @SmallTest
    @UiThreadTest
    @Feature({"DataReduction"})
    public void testUpdateReductionStatisticsEnabledToday() {
        DataReductionStatsPreference pref = new DataReductionStatsPreference(mContext, null);
        long now = System.currentTimeMillis();
        long lastUpdateTime = now - DateUtils.DAY_IN_MILLIS;
        long dataSaverEnableTime = now - DateUtils.HOUR_IN_MILLIS;
        mSettings.setDataReductionLastUpdateTime(lastUpdateTime);
        SharedPreferencesManager.getInstance().writeLong(
                ChromePreferenceKeys.DATA_REDUCTION_FIRST_ENABLED_TIME, dataSaverEnableTime);
        pref.updateReductionStatistics(now);

        Assert.assertEquals(MINIMUM_DAYS_IN_CHART, pref.getNumDaysInChart());
    }

    /**
     * Tests the timespan of the usage graph when Data Saver was enabled yesterday.
     */
    @Test
    @SmallTest
    @UiThreadTest
    @Feature({"DataReduction"})
    public void testUpdateReductionStatisticsEnabledYesterday() {
        DataReductionStatsPreference pref = new DataReductionStatsPreference(mContext, null);
        long now = System.currentTimeMillis();
        long lastUpdateTime = now - DateUtils.DAY_IN_MILLIS;
        long dataSaverEnableTime = now - DateUtils.DAY_IN_MILLIS;
        mSettings.setDataReductionLastUpdateTime(lastUpdateTime);
        SharedPreferencesManager.getInstance().writeLong(
                ChromePreferenceKeys.DATA_REDUCTION_FIRST_ENABLED_TIME, dataSaverEnableTime);
        pref.updateReductionStatistics(now);

        Assert.assertEquals(MINIMUM_DAYS_IN_CHART, pref.getNumDaysInChart());
    }

    /**
     * Tests the timespan of the usage graph when Data Saver was enabled 31 days ago.
     */
    @Test
    @SmallTest
    @UiThreadTest
    @Feature({"DataReduction"})
    public void testUpdateReductionStatisticsEnabled31DaysAgo() {
        DataReductionStatsPreference pref = new DataReductionStatsPreference(mContext, null);
        long now = System.currentTimeMillis();
        long lastUpdateTime = now - DateUtils.DAY_IN_MILLIS;
        long dataSaverEnableTime = now - 31 * DateUtils.DAY_IN_MILLIS;
        mSettings.setDataReductionLastUpdateTime(lastUpdateTime);
        SharedPreferencesManager.getInstance().writeLong(
                ChromePreferenceKeys.DATA_REDUCTION_FIRST_ENABLED_TIME, dataSaverEnableTime);
        pref.updateReductionStatistics(now);

        Assert.assertEquals(MAXIMUM_DAYS_IN_CHART, pref.getNumDaysInChart());
    }

    /**
     * Tests the timespan of the usage graph when the stats have not been
     * updated recently.
     */
    @Test
    @SmallTest
    @UiThreadTest
    @Feature({"DataReduction"})
    public void testUpdateReductionStatisticsStatsNotUpdatedRecently() {
        DataReductionStatsPreference pref = new DataReductionStatsPreference(mContext, null);
        long now = System.currentTimeMillis();
        long lastUpdateTime = now - 7 * DateUtils.DAY_IN_MILLIS;
        int numDaysDataSaverEnabled = 10;
        long dataSaverEnableTime = now - numDaysDataSaverEnabled * DateUtils.DAY_IN_MILLIS;
        mSettings.setDataReductionLastUpdateTime(lastUpdateTime);
        SharedPreferencesManager.getInstance().writeLong(
                ChromePreferenceKeys.DATA_REDUCTION_FIRST_ENABLED_TIME, dataSaverEnableTime);
        pref.updateReductionStatistics(now);

        Assert.assertEquals(numDaysDataSaverEnabled + 1, pref.getNumDaysInChart());
    }

    @Test
    @SmallTest
    @UiThreadTest
    @Feature({"DataReduction"})
    public void testShouldShowRealDataWhenEnoughDataIsUsed() {
        DataReductionStatsPreference pref = new DataReductionStatsPreference(mContext, null);
        long now = System.currentTimeMillis();
        long lastUpdateTime = now - DateUtils.DAY_IN_MILLIS;
        long dataSaverEnableTime = now - DateUtils.HOUR_IN_MILLIS;
        mSettings.setDataReductionLastUpdateTime(lastUpdateTime);
        SharedPreferencesManager.getInstance().writeLong(
                ChromePreferenceKeys.DATA_REDUCTION_FIRST_ENABLED_TIME, dataSaverEnableTime);

        // User has only used 50KB so far.
        mSettings.setReceivedNetworkStatsHistory(new long[] {50 * 1024});
        pref.updateReductionStatistics(now);

        Assert.assertFalse(pref.shouldShowRealData());

        // User has now used 100KB.
        mSettings.setReceivedNetworkStatsHistory(new long[] {100 * 1024});
        pref.updateReductionStatistics(now);

        Assert.assertTrue(pref.shouldShowRealData());
    }
}
