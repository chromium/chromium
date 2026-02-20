// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme.daily_refresh;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.Matrix;
import android.graphics.Point;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.FakeTimeTestRule;
import org.chromium.base.TimeUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.base.test.util.Features;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundType;
import org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpThemeColorInfo.NtpThemeColorId;
import org.chromium.chrome.browser.ntp_customization.theme.theme_collections.CustomBackgroundInfo;
import org.chromium.chrome.browser.ntp_customization.theme.upload_image.BackgroundImageInfo;
import org.chromium.url.JUnitTestGURLs;

import java.io.File;

/** Unit tests for {@link NtpThemeDailyRefreshManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Features.EnableFeatures({ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_V2})
public class NtpThemeDailyRefreshManagerUnitTest {
    @Rule public MockitoRule mMockitoJUnit = MockitoJUnit.rule();
    @Rule public FakeTimeTestRule mFakeTime = new FakeTimeTestRule();

    @Mock private Runnable mOnDailyRefreshThemeCollectionApplied;

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
        assertFalse(mManager.isDailyRefreshEnabled(NtpBackgroundType.CHROME_COLOR));
    }

    @Test
    public void testIsDailyRefreshEnabled_NotChromeColor() {
        NtpCustomizationUtils.setIsChromeColorDailyRefreshEnabledToSharedPreference(true);
        assertFalse(mManager.isDailyRefreshEnabled(NtpBackgroundType.IMAGE_FROM_DISK));
    }

    @Test
    public void testIsDailyRefreshEnabled_Within24Hours() {
        NtpCustomizationUtils.setIsChromeColorDailyRefreshEnabledToSharedPreference(true);
        NtpCustomizationUtils.setDailyRefreshTimestampToSharedPreference(
                TimeUtils.currentTimeMillis());
        mFakeTime.advanceMillis(1);

        assertFalse(mManager.isDailyRefreshEnabled(NtpBackgroundType.CHROME_COLOR));
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

        assertTrue(mManager.isDailyRefreshEnabled(NtpBackgroundType.CHROME_COLOR));
    }

    @Test
    public void testIsDailyRefreshEnabled_After24Hours() {
        NtpCustomizationUtils.setIsChromeColorDailyRefreshEnabledToSharedPreference(true);
        NtpCustomizationUtils.setDailyRefreshTimestampToSharedPreference(
                TimeUtils.currentTimeMillis());
        mFakeTime.advanceMillis(TimeUtils.MILLISECONDS_PER_DAY + 10);

        assertTrue(mManager.isDailyRefreshEnabled(NtpBackgroundType.CHROME_COLOR));
    }

    @Test
    public void testMaybeApplyDailyRefreshForChromeColor() {
        NtpCustomizationUtils.setIsChromeColorDailyRefreshEnabledToSharedPreference(true);
        mFakeTime.advanceMillis(TimeUtils.MILLISECONDS_PER_DAY + 10);

        int newColorId =
                mManager.maybeApplyDailyRefreshForChromeColor(NtpThemeColorId.NTP_COLORS_ROSE);
        assertEquals(NtpThemeColorId.NTP_COLORS_ROSE + 1, newColorId);
    }

    @Test
    public void testGetNtpThemeColorId_DailyUpdateAppliedTheme() {
        mManager.setDailyUpdateStatusForChromeColor(
                NtpThemeColorId.NTP_COLORS_BLUE, TimeUtils.currentTimeMillis());
        assertEquals(
                NtpThemeColorId.NTP_COLORS_BLUE, mManager.getNtpThemeColorIdForChromeColorTheme());
    }

    @Test
    public void testGetNtpThemeColorId_NoDailyUpdateAppliedTheme() {
        NtpCustomizationUtils.setNtpThemeColorIdToSharedPreference(
                NtpThemeColorId.NTP_COLORS_FUCHSIA);
        assertEquals(
                NtpThemeColorId.NTP_COLORS_FUCHSIA,
                mManager.getNtpThemeColorIdForChromeColorTheme());
    }

    @Test
    public void testMaybeSaveDailyRefreshAndReset_forChromeColor() {
        long timeStamp = 100;
        mManager.setDailyUpdateStatusForChromeColor(NtpThemeColorId.NTP_COLORS_AQUA, timeStamp);
        mManager.maybeSaveDailyRefreshAndReset(mOnDailyRefreshThemeCollectionApplied);

        assertEquals(100, NtpCustomizationUtils.getDailyRefreshTimestampToSharedPreference());
        assertEquals(
                NtpThemeColorId.NTP_COLORS_AQUA,
                NtpCustomizationUtils.getNtpThemeColorIdFromSharedPreference());

        assertNull(mManager.getLastDailyUpdateTimestampForTesting());
        assertNull(mManager.getNtpThemeColorIdForTesting());
        assertFalse(mManager.getIsDailyUpdateAppliedForTesting());
        verify(mOnDailyRefreshThemeCollectionApplied, never()).run();
    }

    @Test
    public void testIsDailyRefreshEnabled_themeCollection() {
        // Test case 1: Daily refresh is disabled in CustomBackgroundInfo.
        CustomBackgroundInfo infoDisabled =
                new CustomBackgroundInfo(
                        JUnitTestGURLs.URL_1,
                        /* collectionId= */ "id",
                        /* isUploadedImage= */ false,
                        /* isDailyRefreshEnabled= */ false);
        NtpCustomizationUtils.setCustomBackgroundInfoToSharedPreference(infoDisabled);
        assertFalse(
                "Daily refresh should be disabled if not set in CustomBackgroundInfo.",
                mManager.isDailyRefreshEnabled(NtpBackgroundType.THEME_COLLECTION));

        // Test case 2: Daily refresh is enabled, but it's within 24 hours since the last refresh.
        CustomBackgroundInfo infoEnabled =
                new CustomBackgroundInfo(
                        JUnitTestGURLs.URL_1,
                        /* collectionId= */ "id",
                        /* isUploadedImage= */ false,
                        /* isDailyRefreshEnabled= */ true);
        NtpCustomizationUtils.setCustomBackgroundInfoToSharedPreference(infoEnabled);
        NtpCustomizationUtils.setDailyRefreshTimestampToSharedPreference(
                TimeUtils.currentTimeMillis());
        mFakeTime.advanceMillis(1);
        assertFalse(
                "Daily refresh should be disabled within 24 hours.",
                mManager.isDailyRefreshEnabled(NtpBackgroundType.THEME_COLLECTION));

        // Test case 3: Daily refresh is enabled, and it's been more than 24 hours.
        mFakeTime.advanceMillis(TimeUtils.MILLISECONDS_PER_DAY + 10);
        assertTrue(
                "Daily refresh should be enabled after 24 hours.",
                mManager.isDailyRefreshEnabled(NtpBackgroundType.THEME_COLLECTION));
    }

    @Test
    public void testMaybeApplyDailyRefreshForThemeCollection() {
        CustomBackgroundInfo info =
                new CustomBackgroundInfo(
                        JUnitTestGURLs.URL_1,
                        /* collectionId= */ "id",
                        /* isUploadedImage= */ false,
                        /* isDailyRefreshEnabled= */ true);
        NtpCustomizationUtils.setCustomBackgroundInfoToSharedPreference(info);
        mFakeTime.advanceMillis(TimeUtils.MILLISECONDS_PER_DAY + 10);
        assertFalse(mManager.getIsDailyUpdateAppliedForTesting());

        mManager.maybeApplyDailyRefreshForThemeCollection();

        assertTrue(mManager.getIsDailyUpdateAppliedForTesting());
        assertNotNull(mManager.getLastDailyUpdateTimestampForTesting());
        assertNull(mManager.getNtpThemeColorIdForTesting());
    }

    @Test
    public void testGettersForThemeCollection_noDailyUpdate() {
        // Set up regular theme info.
        NtpCustomizationUtils.setCustomizedPrimaryColorToSharedPreference(Color.RED);
        BackgroundImageInfo imageInfo =
                new BackgroundImageInfo(
                        new Matrix(),
                        new Matrix(),
                        /* portraitWindowSize= */ null,
                        /* landscapeWindowSize= */ null);
        NtpCustomizationUtils.updateBackgroundImageInfo(imageInfo);
        CustomBackgroundInfo customInfo =
                new CustomBackgroundInfo(
                        JUnitTestGURLs.URL_1,
                        /* collectionId= */ "id",
                        /* isUploadedImage= */ false,
                        /* isDailyRefreshEnabled= */ false);
        NtpCustomizationUtils.setCustomBackgroundInfoToSharedPreference(customInfo);

        // Verify getters return regular info.
        assertEquals(Color.RED, mManager.getNtpThemeColorForThemeCollection());
        assertEquals(
                imageInfo.getPortraitMatrix(),
                mManager.getNtpBackgroundImageInfoForThemeCollection().getPortraitMatrix());
        assertEquals(
                customInfo.collectionId,
                mManager.getNtpCustomBackgroundInfoForThemeCollection().collectionId);
    }

    @Test
    public void testGettersForThemeCollection_withDailyUpdate() {
        // Set up regular theme info and daily refresh info.
        NtpCustomizationUtils.setCustomizedPrimaryColorToSharedPreference(Color.RED);
        NtpCustomizationUtils.setDailyRefreshCustomizedPrimaryColorToSharedPreference(Color.BLUE);
        BackgroundImageInfo dailyRefreshImageInfo =
                new BackgroundImageInfo(
                        new Matrix(), new Matrix(), new Point(1, 1), new Point(2, 2));
        NtpCustomizationUtils.updateDailyRefreshBackgroundImageInfo(dailyRefreshImageInfo);
        CustomBackgroundInfo dailyRefreshCustomInfo =
                new CustomBackgroundInfo(
                        JUnitTestGURLs.URL_2,
                        /* collectionId= */ "id_daily",
                        /* isUploadedImage= */ false,
                        /* isDailyRefreshEnabled= */ true);
        NtpCustomizationUtils.setDailyRefreshCustomBackgroundInfoToSharedPreference(
                dailyRefreshCustomInfo);

        // Apply daily update.
        mManager.setDailyUpdateStatusForThemeCollection(TimeUtils.currentTimeMillis());

        // Verify getters return daily refresh info.
        assertEquals(Color.BLUE, mManager.getNtpThemeColorForThemeCollection());
        assertEquals(
                dailyRefreshImageInfo.getPortraitMatrix(),
                mManager.getNtpBackgroundImageInfoForThemeCollection().getPortraitMatrix());
        assertEquals(
                dailyRefreshImageInfo.getPortraitWindowSize(),
                mManager.getNtpBackgroundImageInfoForThemeCollection().getPortraitWindowSize());
        assertEquals(
                dailyRefreshCustomInfo.collectionId,
                mManager.getNtpCustomBackgroundInfoForThemeCollection().collectionId);
    }

    @Test
    public void testMaybeSaveDailyRefreshAndReset_forThemeCollection() {
        // 1. Set background type to THEME_COLLECTION.
        NtpCustomizationUtils.setNtpBackgroundTypeToSharedPreference(
                NtpBackgroundType.THEME_COLLECTION);
        // 2. Set up daily refresh info. This is what will be "committed".
        NtpCustomizationUtils.setDailyRefreshCustomizedPrimaryColorToSharedPreference(Color.BLUE);
        File dailyRefreshFile = NtpCustomizationUtils.createDailyRefreshBackgroundImageFile();
        NtpCustomizationUtils.saveBitmapImageToFile(
                Bitmap.createBitmap(1, 1, Bitmap.Config.ARGB_8888), dailyRefreshFile);
        RobolectricUtil.runAllBackgroundAndUi(); // Wait for async file operations.
        assertTrue(dailyRefreshFile.exists());

        // 3. Apply daily update status in the manager.
        long timeStamp = 100;
        mManager.setDailyUpdateStatusForThemeCollection(timeStamp);

        // 4. Call the method.
        mManager.maybeSaveDailyRefreshAndReset(mOnDailyRefreshThemeCollectionApplied);
        RobolectricUtil.runAllBackgroundAndUi(); // Wait for rename.

        // 5. Verify results.
        assertEquals(timeStamp, NtpCustomizationUtils.getDailyRefreshTimestampToSharedPreference());
        assertEquals(
                Color.BLUE, NtpCustomizationUtils.getCustomizedPrimaryColorFromSharedPreference());
        // Verify file was moved.
        assertTrue(NtpCustomizationUtils.createBackgroundImageFile().exists());
        assertFalse(dailyRefreshFile.exists());

        verify(mOnDailyRefreshThemeCollectionApplied).run();

        // Verify manager state is reset.
        assertNull(mManager.getLastDailyUpdateTimestampForTesting());
        assertNull(mManager.getNtpThemeColorIdForTesting());
        assertFalse(mManager.getIsDailyUpdateAppliedForTesting());
    }
}
