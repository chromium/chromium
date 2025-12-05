// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme.daily_refresh;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.FakeTimeTestRule;
import org.chromium.base.TimeUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundImageType;
import org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpThemeColorInfo.NtpThemeColorId;

/** Unit tests for {@link NtpThemeDailyRefreshManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Features.EnableFeatures({ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_V2})
public class NtpThemeDailyRefreshManagerUnitTest {
    @Rule public MockitoRule mMockitoJUnit = MockitoJUnit.rule();
    @Rule public FakeTimeTestRule mFakeTime = new FakeTimeTestRule();

    private NtpThemeDailyRefreshManager mManager;

    @Before
    public void setUp() {
        mManager = new NtpThemeDailyRefreshManager();
    }

    @After
    public void tearDown() {
        NtpCustomizationUtils.resetSharedPreferenceForTesting();
        mFakeTime.resetTimes();
    }

    @Test
    public void testGetNextColorId() {
        assertEquals(
                NtpThemeColorId.DEFAULT + 2,
                NtpThemeDailyRefreshManager.getNextColorId(NtpThemeColorId.DEFAULT + 1));
        assertEquals(
                NtpThemeColorId.NTP_COLORS_FUCHSIA,
                NtpThemeDailyRefreshManager.getNextColorId(NtpThemeColorId.NTP_COLORS_ROSE));
        assertEquals(
                NtpThemeColorId.DEFAULT + 1,
                NtpThemeDailyRefreshManager.getNextColorId(NtpThemeColorId.NUM_ENTRIES - 1));
    }

    @Test
    public void testIsDailyRefreshEnabled_Disabled() {
        NtpCustomizationUtils.setIsChromeColorDailyRefreshEnabledToSharedPreference(false);
        assertFalse(mManager.isDailyRefreshEnabled(NtpBackgroundImageType.CHROME_COLOR));
    }

    @Test
    public void testIsDailyRefreshEnabled_NotChromeColor() {
        NtpCustomizationUtils.setIsChromeColorDailyRefreshEnabledToSharedPreference(true);
        assertFalse(mManager.isDailyRefreshEnabled(NtpBackgroundImageType.IMAGE_FROM_DISK));
    }

    @Test
    public void testIsDailyRefreshEnabled_Within24Hours() {
        NtpCustomizationUtils.setIsChromeColorDailyRefreshEnabledToSharedPreference(true);
        NtpCustomizationUtils.setDailyRefreshTimestampToSharedPreference(
                TimeUtils.currentTimeMillis());
        mFakeTime.advanceMillis(1);

        assertFalse(mManager.isDailyRefreshEnabled(NtpBackgroundImageType.CHROME_COLOR));
    }

    @Test
    @Features.EnableFeatures({
        ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_V2 + ":daily_refresh_threshold_ms/0"
    })
    public void testIsDailyRefreshEnabled_Within24Hours_ForceUpdate() {
        NtpCustomizationUtils.setIsChromeColorDailyRefreshEnabledToSharedPreference(true);
        NtpCustomizationUtils.setDailyRefreshTimestampToSharedPreference(
                TimeUtils.currentTimeMillis());
        mFakeTime.advanceMillis(1);

        assertTrue(mManager.isDailyRefreshEnabled(NtpBackgroundImageType.CHROME_COLOR));
    }

    @Test
    public void testIsDailyRefreshEnabled_After24Hours() {
        NtpCustomizationUtils.setIsChromeColorDailyRefreshEnabledToSharedPreference(true);
        NtpCustomizationUtils.setDailyRefreshTimestampToSharedPreference(
                TimeUtils.currentTimeMillis());
        mFakeTime.advanceMillis(TimeUtils.MILLISECONDS_PER_DAY + 10);

        assertTrue(mManager.isDailyRefreshEnabled(NtpBackgroundImageType.CHROME_COLOR));
    }

    @Test
    public void testMayApplyDailyRefreshForChromeColor() {
        NtpCustomizationUtils.setIsChromeColorDailyRefreshEnabledToSharedPreference(true);
        mFakeTime.advanceMillis(TimeUtils.MILLISECONDS_PER_DAY + 10);

        int newColorId =
                mManager.mayApplyDailyRefreshForChromeColor(NtpThemeColorId.NTP_COLORS_ROSE);
        assertEquals(NtpThemeColorId.NTP_COLORS_ROSE + 1, newColorId);
    }

    @Test
    public void testGetNtpThemeColorIdForChromeColorTheme_DailyUpdateApplied() {
        mManager.setDailyUpdateStatus(
                NtpThemeColorId.NTP_COLORS_BLUE, TimeUtils.currentTimeMillis());
        assertEquals(
                NtpThemeColorId.NTP_COLORS_BLUE, mManager.getNtpThemeColorIdForChromeColorTheme());
    }

    @Test
    public void testGetNtpThemeColorIdForChromeColorTheme_NoDailyUpdate() {
        NtpCustomizationUtils.setNtpThemeColorIdToSharedPreference(
                NtpThemeColorId.NTP_COLORS_FUCHSIA);
        assertEquals(
                NtpThemeColorId.NTP_COLORS_FUCHSIA,
                mManager.getNtpThemeColorIdForChromeColorTheme());
    }

    @Test
    public void testMaybeSaveDailyRefreshAndReset() {
        long timeStamp = 100;
        mManager.setDailyUpdateStatus(NtpThemeColorId.NTP_COLORS_AQUA, timeStamp);
        mManager.maybeSaveDailyRefreshAndReset();

        assertEquals(100, NtpCustomizationUtils.getDailyRefreshTimestampToSharedPreference());
        assertEquals(
                NtpThemeColorId.NTP_COLORS_AQUA,
                NtpCustomizationUtils.getNtpThemeColorIdFromSharedPreference());

        assertNull(mManager.getLastDailyUpdateTimestampForTesting());
        assertNull(mManager.getNtpThemeColorIdForTesting());
        assertFalse(mManager.getIsDailyUpdateAppliedForTesting());
    }
}
