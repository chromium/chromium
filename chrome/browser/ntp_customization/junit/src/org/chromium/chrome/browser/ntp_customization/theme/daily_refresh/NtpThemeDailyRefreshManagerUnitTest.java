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
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.Matrix;
import android.graphics.Point;

import androidx.annotation.Nullable;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

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
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.url.JUnitTestGURLs;

import java.io.File;

/** Unit tests for {@link NtpThemeDailyRefreshManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@Features.EnableFeatures({ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_V2})
public class NtpThemeDailyRefreshManagerUnitTest {
    private static final String COLLECTION_ID = "id";
    private static final String DAILY_REFRESH_COLLECTION_ID = "id_daily";

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
        NtpCustomizationUtils.maybeDeleteFile(
                NtpCustomizationUtils.createDailyRefreshBackgroundImageFile());
        NtpCustomizationUtils.maybeDeleteFile(NtpCustomizationUtils.createBackgroundImageFile());
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
    public void testDiscardPendingDailyRefresh() {
        NtpCustomizationUtils.setNtpThemeColorIdToSharedPreference(NtpThemeColorId.NTP_COLORS_BLUE);
        mManager.setDailyUpdateStatusForChromeColor(
                NtpThemeColorId.NTP_COLORS_AQUA, /* timestamp= */ 100);
        assertTrue(mManager.getIsDailyUpdateAppliedForTesting());

        mManager.discardPendingDailyRefresh();

        // The getter falls back to the persisted color, and nothing is left to save.
        assertEquals(
                NtpThemeColorId.NTP_COLORS_BLUE, mManager.getNtpThemeColorIdForChromeColorTheme());
        mManager.maybeSaveDailyRefreshAndReset(mOnDailyRefreshThemeCollectionApplied);
        assertEquals(
                NtpThemeColorId.NTP_COLORS_BLUE,
                NtpCustomizationUtils.getNtpThemeColorIdFromSharedPreference());
        assertPendingDailyRefreshDropped();
    }

    @Test
    public void testDiscardPendingDailyRefresh_clearsSkippedState() {
        setDailyRefreshThemeCollection();
        mFakeTime.advanceMillis(TimeUtils.MILLISECONDS_PER_DAY + 10);
        // No prefetched image, so the refresh is skipped.
        mManager.maybeApplyDailyRefreshForThemeCollection();
        assertTrue(mManager.getShouldSkipDailyRefreshForThemeCollectionForTesting());

        mManager.discardPendingDailyRefresh();

        mManager.maybeSaveDailyRefreshAndReset(mOnDailyRefreshThemeCollectionApplied);
        assertPendingDailyRefreshDropped();
    }

    @Test
    public void testIsDailyRefreshEnabled_themeCollection() {
        // Test case 1: Daily refresh is disabled in CustomBackgroundInfo.
        CustomBackgroundInfo infoDisabled =
                new CustomBackgroundInfo(
                        JUnitTestGURLs.URL_1,
                        COLLECTION_ID,
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
                        COLLECTION_ID,
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
        setDailyRefreshThemeCollection();
        // The next image has been prefetched.
        setPrefetchedDailyRefreshInfo();
        mFakeTime.advanceMillis(TimeUtils.MILLISECONDS_PER_DAY + 10);
        assertFalse(mManager.getIsDailyUpdateAppliedForTesting());

        mManager.maybeApplyDailyRefreshForThemeCollection();

        assertTrue(mManager.getIsDailyUpdateAppliedForTesting());
        assertNotNull(mManager.getLastDailyUpdateTimestampForTesting());
        assertNull(mManager.getNtpThemeColorIdForTesting());
        assertEquals(
                DAILY_REFRESH_COLLECTION_ID,
                mManager.getNtpCustomBackgroundInfoForThemeCollection().collectionId);
    }

    @Test
    public void testMaybeApplyDailyRefreshForThemeCollection_noDailyRefreshInfo_skipped() {
        setDailyRefreshThemeCollection();
        NtpCustomizationUtils.setCustomizedPrimaryColorToSharedPreference(Color.RED);
        // A stale or partially written daily refresh color without the daily refresh info.
        NtpCustomizationUtils.setDailyRefreshCustomizedPrimaryColorToSharedPreference(Color.BLUE);
        long lastTimestamp = TimeUtils.currentTimeMillis();
        NtpCustomizationUtils.setDailyRefreshTimestampToSharedPreference(lastTimestamp);
        mFakeTime.advanceMillis(TimeUtils.MILLISECONDS_PER_DAY + 10);

        mManager.maybeApplyDailyRefreshForThemeCollection();

        // The daily refresh is skipped and the getters keep returning the current theme.
        assertFalse(mManager.getIsDailyUpdateAppliedForTesting());
        assertTrue(mManager.getShouldSkipDailyRefreshForThemeCollectionForTesting());
        assertEquals(
                Long.valueOf(TimeUtils.currentTimeMillis()),
                mManager.getLastDailyUpdateTimestampForTesting());
        assertEquals(Integer.valueOf(Color.RED), mManager.getNtpThemeColorForThemeCollection());
        assertEquals(
                COLLECTION_ID,
                mManager.getNtpCustomBackgroundInfoForThemeCollection().collectionId);
        assertEquals(
                NtpBackgroundType.THEME_COLLECTION,
                NtpCustomizationUtils.getNtpBackgroundTypeFromSharedPreference());
        // Nothing is persisted until maybeSaveDailyRefreshAndReset().
        assertEquals(
                lastTimestamp, NtpCustomizationUtils.getDailyRefreshTimestampToSharedPreference());

        // The re-fetch completes later in the same process, e.g., before another activity is
        // created. The daily refresh isn't applied after the current image has been loaded.
        setPrefetchedDailyRefreshInfo();
        mFakeTime.advanceMillis(1);
        mManager.maybeApplyDailyRefreshForThemeCollection();

        assertFalse(mManager.getIsDailyUpdateAppliedForTesting());
        assertTrue(mManager.getShouldSkipDailyRefreshForThemeCollectionForTesting());
        assertEquals(
                COLLECTION_ID,
                mManager.getNtpCustomBackgroundInfoForThemeCollection().collectionId);
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
                        COLLECTION_ID,
                        /* isUploadedImage= */ false,
                        /* isDailyRefreshEnabled= */ false);
        NtpCustomizationUtils.setCustomBackgroundInfoToSharedPreference(customInfo);

        // Verify getters return regular info.
        assertEquals(Integer.valueOf(Color.RED), mManager.getNtpThemeColorForThemeCollection());
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
                        DAILY_REFRESH_COLLECTION_ID,
                        /* isUploadedImage= */ false,
                        /* isDailyRefreshEnabled= */ true);
        NtpCustomizationUtils.setDailyRefreshCustomBackgroundInfoToSharedPreference(
                dailyRefreshCustomInfo);

        // Apply daily update.
        mManager.setDailyUpdateStatusForThemeCollection(
                TimeUtils.currentTimeMillis(), /* shouldApply= */ true);

        // Verify getters return daily refresh info.
        assertEquals(Integer.valueOf(Color.BLUE), mManager.getNtpThemeColorForThemeCollection());
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
        setPrefetchedDailyRefreshInfo();
        NtpCustomizationUtils.setDailyRefreshCustomizedPrimaryColorToSharedPreference(Color.BLUE);
        File dailyRefreshFile = NtpCustomizationUtils.createDailyRefreshBackgroundImageFile();
        NtpCustomizationUtils.saveBitmapImageToFile(
                Bitmap.createBitmap(1, 1, Bitmap.Config.ARGB_8888), dailyRefreshFile);
        RobolectricUtil.runAllBackgroundAndUi(); // Wait for async file operations.
        assertTrue(dailyRefreshFile.exists());

        // 3. Apply daily update status in the manager.
        long timeStamp = 100;
        mManager.setDailyUpdateStatusForThemeCollection(timeStamp, /* shouldApply= */ true);

        // 4. Call the method.
        mManager.maybeSaveDailyRefreshAndReset(mOnDailyRefreshThemeCollectionApplied);
        RobolectricUtil.runAllBackgroundAndUi(); // Wait for rename.

        // 5. Verify results.
        assertEquals(timeStamp, NtpCustomizationUtils.getDailyRefreshTimestampToSharedPreference());
        assertEquals(
                Integer.valueOf(Color.BLUE),
                NtpCustomizationUtils.getCustomizedPrimaryColorFromSharedPreference());
        assertEquals(
                DAILY_REFRESH_COLLECTION_ID,
                NtpCustomizationUtils.getCustomBackgroundInfoFromSharedPreference().collectionId);
        // Verify file was moved.
        assertTrue(NtpCustomizationUtils.createBackgroundImageFile().exists());
        assertFalse(dailyRefreshFile.exists());

        verify(mOnDailyRefreshThemeCollectionApplied).run();

        // Verify manager state is reset.
        assertNull(mManager.getLastDailyUpdateTimestampForTesting());
        assertNull(mManager.getNtpThemeColorIdForTesting());
        assertFalse(mManager.getIsDailyUpdateAppliedForTesting());
    }

    @Test
    public void testMaybeSaveDailyRefreshAndReset_forThemeCollection_noDailyRefreshInfo() {
        File dailyRefreshFile = NtpCustomizationUtils.createDailyRefreshBackgroundImageFile();
        File mainFile = NtpCustomizationUtils.createBackgroundImageFile();
        setDailyRefreshThemeCollection();
        NtpCustomizationUtils.setCustomizedPrimaryColorToSharedPreference(Color.RED);
        // A stale daily refresh image file without the daily refresh info.
        NtpCustomizationUtils.saveBitmapImageToFile(
                Bitmap.createBitmap(1, 1, Bitmap.Config.ARGB_8888), dailyRefreshFile);
        RobolectricUtil.runAllBackgroundAndUi(); // Wait for async file operations.
        assertTrue(dailyRefreshFile.exists());
        assertFalse(mainFile.exists());

        mFakeTime.advanceMillis(TimeUtils.MILLISECONDS_PER_DAY + 10);
        long now = TimeUtils.currentTimeMillis();

        mManager.maybeApplyDailyRefreshForThemeCollection();
        mManager.maybeSaveDailyRefreshAndReset(mOnDailyRefreshThemeCollectionApplied);
        RobolectricUtil.runAllBackgroundAndUi(); // Wait for a possible rename.

        // The next image is fetched again.
        verify(mOnDailyRefreshThemeCollectionApplied).run();
        assertEquals(now, NtpCustomizationUtils.getDailyRefreshTimestampToSharedPreference());
        // Nothing is committed: the current theme is kept and the stale file isn't renamed.
        assertEquals(
                Integer.valueOf(Color.RED),
                NtpCustomizationUtils.getCustomizedPrimaryColorFromSharedPreference());
        assertEquals(
                COLLECTION_ID,
                NtpCustomizationUtils.getCustomBackgroundInfoFromSharedPreference().collectionId);
        assertTrue(dailyRefreshFile.exists());
        assertFalse(mainFile.exists());

        // Verify manager state is reset.
        assertNull(mManager.getLastDailyUpdateTimestampForTesting());
        assertFalse(mManager.getIsDailyUpdateAppliedForTesting());
        assertFalse(mManager.getShouldSkipDailyRefreshForThemeCollectionForTesting());

        // A second call is a no-op.
        mManager.maybeSaveDailyRefreshAndReset(mOnDailyRefreshThemeCollectionApplied);
        verify(mOnDailyRefreshThemeCollectionApplied, times(1)).run();
    }

    @Test
    public void testReadNtpBackgroundImageForThemeCollection_dailyUpdateApplied() {
        testReadNtpBackgroundImageForThemeCollectionImpl(
                /* isDailyUpdateApplied= */ true, /* filePath= */ null);
    }

    @Test
    public void testReadNtpBackgroundImageForThemeCollection_dailyUpdateNotApplied() {
        File customFile =
                NtpCustomizationUtils.createThemeCollectionImageFileInDirForTesting("test_read");
        testReadNtpBackgroundImageForThemeCollectionImpl(
                /* isDailyUpdateApplied= */ false, customFile.getAbsolutePath());
    }

    private void testReadNtpBackgroundImageForThemeCollectionImpl(
            boolean isDailyUpdateApplied, @Nullable String filePath) {
        Bitmap bitmap = Bitmap.createBitmap(1, 1, Bitmap.Config.ARGB_8888);
        File targetFile =
                isDailyUpdateApplied
                        ? NtpCustomizationUtils.createDailyRefreshBackgroundImageFile()
                        : new File(filePath);
        NtpCustomizationUtils.saveBitmapImageToFile(bitmap, targetFile);
        RobolectricUtil.runAllBackgroundAndUi();

        if (isDailyUpdateApplied) {
            mManager.setDailyUpdateStatusForThemeCollection(100, /* shouldApply= */ true);
        }
        mManager.readNtpBackgroundImageForThemeCollection(
                (result) -> assertNotNull(result), /* executor= */ Runnable::run, filePath);
        RobolectricUtil.runAllBackgroundAndUi();
    }

    /** Sets the background to the theme collection {@link #COLLECTION_ID} with daily refresh. */
    private static void setDailyRefreshThemeCollection() {
        NtpCustomizationUtils.setNtpBackgroundTypeToSharedPreference(
                NtpBackgroundType.THEME_COLLECTION);
        NtpCustomizationUtils.setCustomBackgroundInfoToSharedPreference(
                new CustomBackgroundInfo(
                        JUnitTestGURLs.URL_1,
                        COLLECTION_ID,
                        /* isUploadedImage= */ false,
                        /* isDailyRefreshEnabled= */ true));
    }

    /** Saves the prefetched daily refresh info of {@link #DAILY_REFRESH_COLLECTION_ID}. */
    private static void setPrefetchedDailyRefreshInfo() {
        NtpCustomizationUtils.setDailyRefreshCustomBackgroundInfoToSharedPreference(
                new CustomBackgroundInfo(
                        JUnitTestGURLs.URL_2,
                        DAILY_REFRESH_COLLECTION_ID,
                        /* isUploadedImage= */ false,
                        /* isDailyRefreshEnabled= */ true));
    }

    /**
     * Asserts that the pending daily refresh was dropped: nothing was persisted or re-fetched, and
     * the manager state is reset.
     */
    private void assertPendingDailyRefreshDropped() {
        verify(mOnDailyRefreshThemeCollectionApplied, never()).run();
        assertFalse(
                ChromeSharedPreferences.getInstance()
                        .contains(
                                ChromePreferenceKeys
                                        .NTP_CUSTOMIZATION_LAST_DAILY_REFRESH_TIMESTAMP));
        assertFalse(mManager.getIsDailyUpdateAppliedForTesting());
        assertFalse(mManager.getShouldSkipDailyRefreshForThemeCollectionForTesting());
        assertNull(mManager.getLastDailyUpdateTimestampForTesting());
        assertNull(mManager.getNtpThemeColorIdForTesting());
    }
}
