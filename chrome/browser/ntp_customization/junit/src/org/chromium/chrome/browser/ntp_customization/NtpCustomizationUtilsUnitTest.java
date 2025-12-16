// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundImageType.COLOR_FROM_HEX;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundImageType.DEFAULT;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundImageType.IMAGE_FROM_DISK;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.getBackground;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.NTP_CUSTOMIZATION_LAST_DAILY_REFRESH_TIMESTAMP;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.Matrix;
import android.graphics.Point;
import android.graphics.drawable.Drawable;
import android.os.Build;
import android.view.ContextThemeWrapper;

import androidx.annotation.ColorInt;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.test.BaseRobolectricTestRule;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundImageType;
import org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpThemeColorFromHexInfo;
import org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpThemeColorInfo;
import org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpThemeColorInfo.NtpThemeColorId;
import org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpThemeColorUtils;
import org.chromium.chrome.browser.ntp_customization.theme.daily_refresh.NtpThemeDailyRefreshManager;
import org.chromium.chrome.browser.ntp_customization.theme.theme_collections.CustomBackgroundInfo;
import org.chromium.chrome.browser.ntp_customization.theme.upload_image.BackgroundImageInfo;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.theme.ThemeUtils;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.image_fetcher.ImageFetcher;
import org.chromium.components.image_fetcher.ImageFetcher.Params;
import org.chromium.ui.util.ColorUtils;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.io.File;

/** Unit tests for {@link NtpCustomizationUtils} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, sdk = Build.VERSION_CODES.R)
public class NtpCustomizationUtilsUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Tab mTab;
    @Mock private Drawable mDrawable;

    private Context mContext;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
    }

    @After
    public void tearDown() {
        // Clean up preferences to not affect other tests.
        NtpCustomizationUtils.resetSharedPreferenceForTesting();
        ColorUtils.setInNightModeForTesting(false);
        // Clean up files.
        NtpCustomizationUtils.deleteBackgroundImageFileImpl(
                NtpCustomizationUtils.createBackgroundImageFile());
        NtpCustomizationUtils.deleteBackgroundImageFileImpl(
                NtpCustomizationUtils.createDailyRefreshBackgroundImageFile());
    }

    @Test
    @SmallTest
    public void testGetBackgroundSizeOne() {
        int resId = getBackground(1, 0);
        assertEquals(R.drawable.ntp_customization_bottom_sheet_list_item_background_single, resId);
    }

    @Test
    @SmallTest
    public void testGetBackgroundSizeTwo() {
        int resId = getBackground(2, 0);
        assertEquals(R.drawable.ntp_customization_bottom_sheet_list_item_background_top, resId);

        resId = getBackground(2, 1);
        assertEquals(R.drawable.ntp_customization_bottom_sheet_list_item_background_bottom, resId);
    }

    @Test
    @SmallTest
    public void testGetBackgroundSizeThree() {
        int resId = getBackground(3, 0);
        assertEquals(R.drawable.ntp_customization_bottom_sheet_list_item_background_top, resId);

        resId = getBackground(3, 1);
        assertEquals(R.drawable.ntp_customization_bottom_sheet_list_item_background_middle, resId);

        resId = getBackground(3, 2);
        assertEquals(R.drawable.ntp_customization_bottom_sheet_list_item_background_bottom, resId);
    }

    @Test
    @SmallTest
    public void testGetBackgroundLargeSize() {
        int listSize = 10;
        int resId = getBackground(listSize, 0);
        assertEquals(R.drawable.ntp_customization_bottom_sheet_list_item_background_top, resId);

        for (int index = 1; index < listSize - 1; index++) {
            resId = getBackground(listSize, index);
            assertEquals(
                    R.drawable.ntp_customization_bottom_sheet_list_item_background_middle, resId);
        }

        resId = getBackground(listSize, listSize - 1);
        assertEquals(R.drawable.ntp_customization_bottom_sheet_list_item_background_bottom, resId);
    }

    @Test
    public void testGetAndSetNtpBackgroundImageType() {
        NtpCustomizationUtils.resetSharedPreferenceForTesting();
        assertEquals(
                NtpBackgroundImageType.DEFAULT,
                NtpCustomizationUtils.getNtpBackgroundImageTypeFromSharedPreference());

        @NtpBackgroundImageType int imageType = IMAGE_FROM_DISK;
        NtpCustomizationUtils.setNtpBackgroundImageTypeToSharedPreference(imageType);

        assertEquals(
                imageType, NtpCustomizationUtils.getNtpBackgroundImageTypeFromSharedPreference());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_V2)
    public void testGetAndSetNtpBackgroundImageType_flagDisabled() {
        NtpCustomizationUtils.setNtpBackgroundImageTypeToSharedPreference(IMAGE_FROM_DISK);

        assertEquals(DEFAULT, NtpCustomizationUtils.getNtpBackgroundImageType());
    }

    @Test
    public void testDeleteBackgroundImageFile() {
        // Saves the bitmap to a file on the disk.
        Bitmap bitmap = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);
        File file = NtpCustomizationUtils.createBackgroundImageFile();
        NtpCustomizationUtils.saveBitmapImageToFile(bitmap, file);
        BaseRobolectricTestRule.runAllBackgroundAndUi(); // Wait for async file operations.

        assertTrue(file.exists());

        NtpCustomizationUtils.deleteBackgroundImageFileImpl(file);
        assertFalse(file.exists());
    }

    @Test
    public void testSaveAndReadBackgroundImage() {
        // Saves the bitmap to a file on the disk.
        Bitmap bitmap = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);
        File file = NtpCustomizationUtils.createBackgroundImageFile();
        NtpCustomizationUtils.saveBitmapImageToFile(bitmap, file);
        BaseRobolectricTestRule.runAllBackgroundAndUi(); // Wait for async file operations.

        // Reads the bitmap from the file.
        Bitmap bitmapResult = NtpCustomizationUtils.readNtpBackgroundImageImpl(file);

        // Verifies that the bitmap read from the file matches the original bitmap.
        assertTrue(bitmap.sameAs(bitmapResult));
    }

    @Test
    @SuppressWarnings("DirectInvocationOnMock")
    public void testSupportsEnableEdgeToEdgeOnTop() {
        assertFalse(NtpCustomizationUtils.supportsEnableEdgeToEdgeOnTop(null));

        when(mTab.isNativePage()).thenReturn(false);
        assertFalse(NtpCustomizationUtils.supportsEnableEdgeToEdgeOnTop(mTab));

        when(mTab.isNativePage()).thenReturn(true);
        NativePage nativePage = mock(NativePage.class);
        when(mTab.getNativePage()).thenReturn(nativePage);
        assertFalse(nativePage.supportsEdgeToEdgeOnTop());
        assertFalse(NtpCustomizationUtils.supportsEnableEdgeToEdgeOnTop(mTab));

        when(nativePage.supportsEdgeToEdgeOnTop()).thenReturn(true);
        assertTrue(NtpCustomizationUtils.supportsEnableEdgeToEdgeOnTop(mTab));
    }

    @Test
    public void testShouldSkipTopInsetChange() {
        assertTrue(
                NtpCustomizationUtils.shouldSkipTopInsetsChange(
                        /* appliedTopPadding= */ 50,
                        /* systemTopInset= */ 50,
                        /* consumeTopInset= */ true));
        assertTrue(
                NtpCustomizationUtils.shouldSkipTopInsetsChange(
                        /* appliedTopPadding= */ 0,
                        /* systemTopInset= */ 50,
                        /* consumeTopInset= */ false));
        // Verifies do NOT skip if NTP should consume top inset while its current layout doesn't
        // have a top padding.
        assertFalse(
                NtpCustomizationUtils.shouldSkipTopInsetsChange(
                        /* appliedTopPadding= */ 0,
                        /* systemTopInset= */ 50,
                        /* consumeTopInset= */ true));
        // Verifies do NOT skip if NTP shouldn't consume top inset while its current layout has a
        // top padding.
        assertFalse(
                NtpCustomizationUtils.shouldSkipTopInsetsChange(
                        /* appliedTopPadding= */ 50,
                        /* systemTopInset= */ 50,
                        /* consumeTopInset= */ false));
    }

    @Test
    public void testUpdateBackgroundColor() {
        @ColorInt int defaultColor = Color.WHITE;
        @ColorInt int color = Color.RED;

        SharedPreferencesManager prefsManager = ChromeSharedPreferences.getInstance();
        prefsManager.removeKey(ChromePreferenceKeys.NTP_CUSTOMIZATION_BACKGROUND_COLOR);
        assertEquals(
                defaultColor,
                NtpCustomizationUtils.getBackgroundColorFromSharedPreference(defaultColor));

        NtpCustomizationUtils.setBackgroundColorToSharedPreference(color);
        assertEquals(
                color, NtpCustomizationUtils.getBackgroundColorFromSharedPreference(defaultColor));

        NtpCustomizationUtils.resetCustomizedColors();
        assertFalse(prefsManager.contains(ChromePreferenceKeys.NTP_CUSTOMIZATION_BACKGROUND_COLOR));
    }

    @Test
    public void testUpdateCustomizedPrimaryColor() {
        @ColorInt int color = Color.BLUE;

        assertEquals(
                NtpThemeColorInfo.COLOR_NOT_SET,
                NtpCustomizationUtils.getCustomizedPrimaryColorFromSharedPreference());

        NtpCustomizationUtils.setCustomizedPrimaryColorToSharedPreference(color);
        assertEquals(color, NtpCustomizationUtils.getCustomizedPrimaryColorFromSharedPreference());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_V2)
    public void testGetPrimaryColorFromCustomizedThemeColor_flagDisabled() {
        assertNull(
                NtpCustomizationUtils.getPrimaryColorFromCustomizedThemeColor(
                        mContext, /* checkDailyRefresh= */ false));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_V2)
    public void testGetPrimaryColorFromCustomizedThemeColor_wrongImageType() {
        NtpCustomizationUtils.setNtpBackgroundImageTypeToSharedPreference(
                NtpBackgroundImageType.DEFAULT);
        NtpCustomizationUtils.setCustomizedPrimaryColorToSharedPreference(Color.RED);
        assertNull(
                NtpCustomizationUtils.getPrimaryColorFromCustomizedThemeColor(
                        mContext, /* checkDailyRefresh= */ false));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_V2)
    public void testGetPrimaryColorFromCustomizedThemeColor_chromeColor_colorNotSet() {
        NtpCustomizationUtils.setNtpBackgroundImageTypeToSharedPreference(
                NtpBackgroundImageType.CHROME_COLOR);
        SharedPreferencesManager prefsManager = ChromeSharedPreferences.getInstance();
        prefsManager.removeKey(ChromePreferenceKeys.NTP_CUSTOMIZATION_PRIMARY_COLOR);

        assertNull(
                NtpCustomizationUtils.getPrimaryColorFromCustomizedThemeColor(
                        mContext, /* checkDailyRefresh= */ false));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_V2)
    public void testGetPrimaryColorFromCustomizedThemeColor_chromeColor_colorSet() {
        @NtpThemeColorId int colorId = NtpThemeColorId.NTP_COLORS_BLUE;
        NtpCustomizationUtils.setNtpBackgroundImageTypeToSharedPreference(
                NtpBackgroundImageType.CHROME_COLOR);
        NtpCustomizationUtils.setNtpThemeColorIdToSharedPreference(colorId);
        @ColorInt int primaryColor = mContext.getColor(R.color.ntp_color_blue_primary);

        assertEquals(
                primaryColor,
                (int)
                        NtpCustomizationUtils.getPrimaryColorFromCustomizedThemeColor(
                                mContext, /* checkDailyRefresh= */ false));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_V2)
    public void testGetPrimaryColorFromCustomizedThemeColor_chromeColor_colorSet_dailyRefresh() {
        @NtpThemeColorId int colorId = NtpThemeColorId.NTP_COLORS_BLUE;
        @NtpThemeColorId int newColorId = NtpThemeColorId.NTP_COLORS_BLUE + 1;

        NtpCustomizationUtils.setNtpBackgroundImageTypeToSharedPreference(
                NtpBackgroundImageType.CHROME_COLOR);
        // Saves the old color id to the SharedPreference.
        NtpCustomizationUtils.setNtpThemeColorIdToSharedPreference(colorId);
        NtpCustomizationUtils.setIsChromeColorDailyRefreshEnabledToSharedPreference(true);

        // Primary color of the new color id.
        @ColorInt
        int primaryColor =
                mContext.getColor(NtpThemeColorUtils.getNtpThemePrimaryColorResId(newColorId));
        // Creates a new instance for the singleton NtpThemeDailyRefreshManager.
        NtpThemeDailyRefreshManager.createInstanceForTesting();

        // Verifies a refreshed color is returned when applying the daily refresh.
        assertEquals(
                primaryColor,
                (int)
                        NtpCustomizationUtils.getPrimaryColorFromCustomizedThemeColor(
                                mContext, /* checkDailyRefresh= */ true));

        // Verifies the new refreshed primary color is returned when not applying daily refresh
        // again.
        assertEquals(
                primaryColor,
                (int)
                        NtpCustomizationUtils.getPrimaryColorFromCustomizedThemeColor(
                                mContext, /* checkDailyRefresh= */ false));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_V2)
    public void testGetPrimaryColorFromCustomizedThemeColor_colorSetWithImage() {
        NtpCustomizationUtils.setNtpBackgroundImageTypeToSharedPreference(IMAGE_FROM_DISK);
        NtpCustomizationUtils.setCustomizedPrimaryColorToSharedPreference(Color.BLUE);

        assertEquals(
                Color.BLUE,
                (int)
                        NtpCustomizationUtils.getPrimaryColorFromCustomizedThemeColor(
                                mContext, /* checkDailyRefresh= */ false));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_V2)
    public void testGetPrimaryColorFromCustomizedThemeColor_themeCollection() {
        NtpThemeDailyRefreshManager.createInstanceForTesting();
        NtpCustomizationUtils.setNtpBackgroundImageTypeToSharedPreference(
                NtpBackgroundImageType.THEME_COLLECTION);

        // Test with daily refresh disabled.
        CustomBackgroundInfo infoNoRefresh =
                new CustomBackgroundInfo(
                        JUnitTestGURLs.URL_1,
                        /* collectionId= */ "id",
                        /* isUploadedImage= */ false,
                        /* isDailyRefreshEnabled= */ false);
        NtpCustomizationUtils.setCustomBackgroundInfoToSharedPreference(infoNoRefresh);
        NtpCustomizationUtils.setCustomizedPrimaryColorToSharedPreference(Color.RED);
        assertEquals(
                "Without daily refresh, it should return the regular color.",
                Color.RED,
                (int)
                        NtpCustomizationUtils.getPrimaryColorFromCustomizedThemeColor(
                                mContext, /* checkDailyRefresh= */ true));

        // Test with daily refresh enabled.
        CustomBackgroundInfo infoWithRefresh =
                new CustomBackgroundInfo(
                        JUnitTestGURLs.URL_1,
                        /* collectionId= */ "id",
                        /* isUploadedImage= */ false,
                        /* isDailyRefreshEnabled= */ true);
        NtpCustomizationUtils.setCustomBackgroundInfoToSharedPreference(infoWithRefresh);
        NtpCustomizationUtils.setDailyRefreshCustomizedPrimaryColorToSharedPreference(Color.BLUE);
        assertEquals(
                "With daily refresh, it should return the daily refresh color.",
                Color.BLUE,
                (int)
                        NtpCustomizationUtils.getPrimaryColorFromCustomizedThemeColor(
                                mContext, /* checkDailyRefresh= */ true));
        // Verify that after daily refresh, getting the color without daily refresh returns the new
        // color.
        assertEquals(
                "After daily refresh, it should return the new color even without check.",
                Color.BLUE,
                (int)
                        NtpCustomizationUtils.getPrimaryColorFromCustomizedThemeColor(
                                mContext, /* checkDailyRefresh= */ false));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_V2)
    public void testLoadColorInfoFromSharedPreference_flagDisabled() {
        assertNull(NtpCustomizationUtils.loadColorInfoFromSharedPreference(mContext));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_V2)
    public void testLoadColorInfoFromSharedPreference_wrongImageType() {
        NtpCustomizationUtils.setNtpBackgroundImageTypeToSharedPreference(
                NtpBackgroundImageType.DEFAULT);
        NtpCustomizationUtils.setCustomizedPrimaryColorToSharedPreference(Color.RED);
        assertNull(NtpCustomizationUtils.loadColorInfoFromSharedPreference(mContext));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_V2)
    public void testLoadColorInfoFromSharedPreference_chromeColor() {
        NtpCustomizationUtils.setNtpBackgroundImageTypeToSharedPreference(
                NtpBackgroundImageType.CHROME_COLOR);
        SharedPreferencesManager prefsManager = ChromeSharedPreferences.getInstance();
        prefsManager.removeKey(ChromePreferenceKeys.NTP_CUSTOMIZATION_THEME_COLOR_ID);

        // Verifies that null is returned when color id isn't set.
        assertNull(NtpCustomizationUtils.loadColorInfoFromSharedPreference(mContext));

        @NtpThemeColorId int colorId = NtpThemeColorId.NTP_COLORS_BLUE;
        NtpThemeColorInfo colorInfo = NtpThemeColorUtils.createNtpThemeColorInfo(mContext, colorId);
        NtpCustomizationUtils.setNtpThemeColorIdToSharedPreference(colorId);

        assertEquals(colorInfo, NtpCustomizationUtils.loadColorInfoFromSharedPreference(mContext));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_V2)
    public void testLoadColorInfoFromSharedPreference_colorFromHex() {
        NtpCustomizationUtils.setNtpBackgroundImageTypeToSharedPreference(COLOR_FROM_HEX);
        @ColorInt int primaryColor = Color.RED;
        @ColorInt int backgroundColor = Color.BLUE;
        NtpCustomizationUtils.setCustomizedPrimaryColorToSharedPreference(primaryColor);
        NtpCustomizationUtils.setBackgroundColorToSharedPreference(backgroundColor);

        // Verifies that both the primary color and background color from the loaded results match.
        NtpThemeColorInfo info = NtpCustomizationUtils.loadColorInfoFromSharedPreference(mContext);
        assertTrue(info instanceof NtpThemeColorFromHexInfo);
        assertEquals(primaryColor, ((NtpThemeColorFromHexInfo) info).primaryColor);
        assertEquals(backgroundColor, ((NtpThemeColorFromHexInfo) info).backgroundColor);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_V2)
    public void testLoadColorInfoFromSharedPreference_colorSetWithImage() {
        NtpCustomizationUtils.setNtpBackgroundImageTypeToSharedPreference(IMAGE_FROM_DISK);
        // Verifies that null is returned if no primary color is set.
        assertNull(NtpCustomizationUtils.loadColorInfoFromSharedPreference(mContext));

        @ColorInt int primaryColor = Color.RED;
        NtpCustomizationUtils.setCustomizedPrimaryColorToSharedPreference(primaryColor);
        NtpCustomizationUtils.setNtpBackgroundImageTypeToSharedPreference(COLOR_FROM_HEX);

        // Verifies that the primary color from the loaded results matches.
        NtpThemeColorInfo info = NtpCustomizationUtils.loadColorInfoFromSharedPreference(mContext);
        assertTrue(info instanceof NtpThemeColorFromHexInfo);
        assertEquals(primaryColor, ((NtpThemeColorFromHexInfo) info).primaryColor);
        assertEquals(
                NtpThemeColorInfo.COLOR_NOT_SET, ((NtpThemeColorFromHexInfo) info).backgroundColor);
    }

    @Test
    public void testUpdateBackgroundImageInfo_SavesToSharedPreferences() {
        Matrix portrait = new Matrix();
        portrait.setTranslate(10, 10);
        Matrix landscape = new Matrix();
        landscape.setScale(2, 2);

        Point portraitSize = new Point(1080, 1920);
        Point landscapeSize = new Point(2000, 1080);
        BackgroundImageInfo info =
                new BackgroundImageInfo(portrait, landscape, portraitSize, landscapeSize);

        NtpCustomizationUtils.updateBackgroundImageInfo(info);

        SharedPreferencesManager prefs = ChromeSharedPreferences.getInstance();
        String storedPortrait =
                prefs.readString(ChromePreferenceKeys.NTP_BACKGROUND_IMAGE_PORTRAIT_INFO, "");
        String storedLandscape =
                prefs.readString(ChromePreferenceKeys.NTP_BACKGROUND_IMAGE_LANDSCAPE_INFO, "");

        assertEquals(
                "Portrait info mismatch",
                "[1.0, 0.0, 10.0, 0.0, 1.0, 10.0, 0.0, 0.0, 1.0]|1080|1920",
                storedPortrait);

        assertEquals(
                "Landscape info mismatch",
                "[2.0, 0.0, 0.0, 0.0, 2.0, 0.0, 0.0, 0.0, 1.0]|2000|1080",
                storedLandscape);
    }

    @Test
    public void testReadNtpBackgroundImageInfo_ParsesCorrectly() {
        SharedPreferencesManager prefs = ChromeSharedPreferences.getInstance();

        // Format: MatrixString|Width|Height
        String portraitImageInfo = "[8.0, 0.0, 0.0, 0.0, 4.0, 0.0, 0.0, 0.0, 1.0]|500|800";
        String landscapeImageInfo =
                "[9.0, 0.0, 0.0, 0.0, 14.0, 0.0, 0.0, 0.0, 1.0]"; // In landscape mode, the window
        // size is not saved in the disk

        prefs.writeString(
                ChromePreferenceKeys.NTP_BACKGROUND_IMAGE_PORTRAIT_INFO, portraitImageInfo);
        prefs.writeString(
                ChromePreferenceKeys.NTP_BACKGROUND_IMAGE_LANDSCAPE_INFO, landscapeImageInfo);

        BackgroundImageInfo result = NtpCustomizationUtils.readNtpBackgroundImageInfo();

        // Verifies the matrices
        assertEquals(
                "Portrait matrix should match input string",
                "[8.0, 0.0, 0.0, 0.0, 4.0, 0.0, 0.0, 0.0, 1.0]",
                BackgroundImageInfo.matrixToString(result.getPortraitMatrix()));
        assertEquals(
                "Landscape matrix should match input string",
                "[9.0, 0.0, 0.0, 0.0, 14.0, 0.0, 0.0, 0.0, 1.0]",
                BackgroundImageInfo.matrixToString(result.getLandscapeMatrix()));

        // Verifies image information in portrait mode
        Point portraitSize = result.getPortraitWindowSize();
        assertEquals(500, portraitSize.x);
        assertEquals(800, portraitSize.y);

        // Verifies image information in landscape mode
        Point landscapeSize = result.getLandscapeWindowSize();
        assertNull(landscapeSize);
    }

    @Test
    public void testResetSharedPreference_ClearsInfoKeys() {
        SharedPreferencesManager prefs = ChromeSharedPreferences.getInstance();
        prefs.writeString(ChromePreferenceKeys.NTP_BACKGROUND_IMAGE_PORTRAIT_INFO, "foo");
        prefs.writeString(ChromePreferenceKeys.NTP_BACKGROUND_IMAGE_LANDSCAPE_INFO, "bar");

        NtpCustomizationUtils.resetSharedPreferenceForTesting();

        assertFalse(prefs.contains(ChromePreferenceKeys.NTP_BACKGROUND_IMAGE_PORTRAIT_INFO));
        assertFalse(prefs.contains(ChromePreferenceKeys.NTP_BACKGROUND_IMAGE_LANDSCAPE_INFO));
    }

    @Test
    public void testGetTintedGoogleLogoDrawable_nonChromeColor() {
        ColorUtils.setInNightModeForTesting(false);
        Drawable mutateDrawable = mock(Drawable.class);
        when(mDrawable.mutate()).thenReturn(mutateDrawable);
        @ColorInt int primaryColor = Color.RED;

        // Test cases in light mode:
        // Verifies that no tint color is set for the default theme in light mode.
        NtpCustomizationUtils.getTintedGoogleLogoDrawableImpl(
                mContext, mDrawable, NtpBackgroundImageType.DEFAULT, primaryColor);
        verify(mutateDrawable, never()).setTint(anyInt());

        // Verifies that color white is set for customized background images.
        NtpCustomizationUtils.getTintedGoogleLogoDrawableImpl(
                mContext, mDrawable, NtpBackgroundImageType.IMAGE_FROM_DISK, primaryColor);
        verify(mutateDrawable).setTint(eq(Color.WHITE));

        // Test cases in dark mode:
        ColorUtils.setInNightModeForTesting(true);
        clearInvocations(mutateDrawable);

        // Verifies that color white is set for customized background images.
        NtpCustomizationUtils.getTintedGoogleLogoDrawableImpl(
                mContext, mDrawable, NtpBackgroundImageType.IMAGE_FROM_DISK, primaryColor);
        verify(mutateDrawable).setTint(eq(Color.WHITE));

        // Verifies that color white is set for the default theme.
        NtpCustomizationUtils.getTintedGoogleLogoDrawableImpl(
                mContext, mDrawable, NtpBackgroundImageType.DEFAULT, primaryColor);
        verify(mutateDrawable, times(2)).setTint(eq(Color.WHITE));
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_V2)
    public void testGetTintedGoogleLogoDrawable_chromeColor_lightMode() {
        // Test cases in light mode.
        ColorUtils.setInNightModeForTesting(false);
        @ColorInt int primaryColor = mContext.getColor(R.color.ntp_color_blue_primary);
        Drawable mutateDrawable = mock(Drawable.class);
        when(mDrawable.mutate()).thenReturn(mutateDrawable);

        // Verifies that the saved primary color is set for customized color themes if exists.
        assertEquals(
                mutateDrawable,
                NtpCustomizationUtils.getTintedGoogleLogoDrawableImpl(
                        mContext, mDrawable, NtpBackgroundImageType.CHROME_COLOR, primaryColor));
        verify(mutateDrawable).setTint(eq(primaryColor));

        clearInvocations(mutateDrawable);
        // Verifies that if primary color is missing, no tint color is set in light mode.
        assertEquals(
                mDrawable,
                NtpCustomizationUtils.getTintedGoogleLogoDrawableImpl(
                        mContext,
                        mDrawable,
                        NtpBackgroundImageType.CHROME_COLOR,
                        /* primaryColor= */ null));
        verify(mutateDrawable, never()).setTint(eq(primaryColor));
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_V2)
    public void testGetTintedGoogleLogoDrawable_chromeColor_darkMode() {
        // Test cases in dark mode.
        ColorUtils.setInNightModeForTesting(true);
        @ColorInt int primaryColor = mContext.getColor(R.color.ntp_color_blue_primary);

        Drawable mutateDrawable = mock(Drawable.class);
        when(mDrawable.mutate()).thenReturn(mutateDrawable);

        // Verifies that the saved primary color is set for customized color themes.
        NtpCustomizationUtils.getTintedGoogleLogoDrawableImpl(
                mContext, mDrawable, NtpBackgroundImageType.CHROME_COLOR, primaryColor);
        verify(mutateDrawable).setTint(eq(primaryColor));

        clearInvocations(mutateDrawable);
        // Verifies when the primary color is missing, color white is set in dark mode.
        NtpCustomizationUtils.getTintedGoogleLogoDrawableImpl(
                mContext, mDrawable, NtpBackgroundImageType.CHROME_COLOR, /* primaryColor= */ null);
        verify(mutateDrawable).setTint(eq(Color.WHITE));
    }

    @Test
    public void testSetAndGetNtpThemeColorIdFromSharedPreference() {
        assertEquals(
                NtpThemeColorId.DEFAULT,
                NtpCustomizationUtils.getNtpThemeColorIdFromSharedPreference());

        @NtpThemeColorId int id = NtpThemeColorId.NTP_COLORS_AQUA;
        NtpCustomizationUtils.setNtpThemeColorIdToSharedPreference(id);
        assertEquals(id, NtpCustomizationUtils.getNtpThemeColorIdFromSharedPreference());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_V2)
    public void testShouldApplyWhiteBackgroundOnSearchBox_flagDisabled() {
        NtpCustomizationConfigManager configManager = new NtpCustomizationConfigManager();
        NtpCustomizationConfigManager.setInstanceForTesting(configManager);

        configManager.setBackgroundImageTypeForTesting(NtpBackgroundImageType.IMAGE_FROM_DISK);

        assertFalse(NtpCustomizationUtils.shouldApplyWhiteBackgroundOnSearchBox());

        configManager.resetForTesting();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_V2)
    public void testShouldApplyWhiteBackgroundOnSearchBox_withType() {
        assertFalse(
                NtpCustomizationUtils.shouldApplyWhiteBackgroundOnSearchBox(
                        NtpBackgroundImageType.DEFAULT));
        assertFalse(
                NtpCustomizationUtils.shouldApplyWhiteBackgroundOnSearchBox(
                        NtpBackgroundImageType.CHROME_COLOR));
        assertFalse(
                NtpCustomizationUtils.shouldApplyWhiteBackgroundOnSearchBox(
                        NtpBackgroundImageType.COLOR_FROM_HEX));

        assertTrue(
                NtpCustomizationUtils.shouldApplyWhiteBackgroundOnSearchBox(
                        NtpBackgroundImageType.IMAGE_FROM_DISK));
        assertTrue(
                NtpCustomizationUtils.shouldApplyWhiteBackgroundOnSearchBox(
                        NtpBackgroundImageType.THEME_COLLECTION));
    }

    @Test
    public void testGetSearchBoxIconColorTint() {
        // Verifies the color tint for customized background images.
        assertEquals(
                AppCompatResources.getColorStateList(mContext, R.color.default_icon_color_dark),
                NtpCustomizationUtils.getSearchBoxIconColorTint(
                        mContext, /* shouldApplyWhiteBackgroundOnSearchBox= */ true));

        // Verifies the color tint for the default theme.
        assertEquals(
                ThemeUtils.getThemedToolbarIconTint(mContext, BrandedColorScheme.APP_DEFAULT),
                NtpCustomizationUtils.getSearchBoxIconColorTint(
                        mContext, /* shouldApplyWhiteBackgroundOnSearchBox= */ false));
    }

    @Test
    public void testGetSearchBoxTextStyleResId() {
        // Verifies the text style for customized background images.
        assertEquals(
                R.style.TextAppearance_ComposeplateTextMediumDark,
                NtpCustomizationUtils.getSearchBoxTextStyleResId(
                        /* shouldApplyWhiteBackgroundOnSearchBox= */ true));

        // Verifies the text style for the default theme.
        assertEquals(
                R.style.TextAppearance_ComposeplateTextMedium,
                NtpCustomizationUtils.getSearchBoxTextStyleResId(
                        /* shouldApplyWhiteBackgroundOnSearchBox= */ false));
    }

    @Test
    public void testFetchThemeCollectionImage() {
        ImageFetcher imageFetcher = mock(ImageFetcher.class);
        GURL imageUrl = JUnitTestGURLs.URL_1;
        Callback<Bitmap> callback = mock(Callback.class);

        NtpCustomizationUtils.fetchThemeCollectionImage(imageFetcher, imageUrl, callback);

        ArgumentCaptor<Params> paramsCaptor = ArgumentCaptor.forClass(ImageFetcher.Params.class);
        verify(imageFetcher).fetchImage(paramsCaptor.capture(), eq(callback));

        ImageFetcher.Params params = paramsCaptor.getValue();
        assertEquals(imageUrl.getSpec(), params.url);
    }

    @Test
    @DisableFeatures(ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_V2)
    public void testShouldAdjustIconTintForNtp_flagDisabled() {
        NtpCustomizationConfigManager configManager = new NtpCustomizationConfigManager();
        NtpCustomizationConfigManager.setInstanceForTesting(configManager);

        configManager.setBackgroundImageTypeForTesting(NtpBackgroundImageType.IMAGE_FROM_DISK);

        assertFalse(NtpCustomizationUtils.shouldAdjustIconTintForNtp(/* isTablet= */ false));

        configManager.resetForTesting();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_V2)
    public void testShouldAdjustIconTintForNtp_isTablet() {
        NtpCustomizationConfigManager configManager = new NtpCustomizationConfigManager();
        NtpCustomizationConfigManager.setInstanceForTesting(configManager);

        configManager.setBackgroundImageTypeForTesting(NtpBackgroundImageType.IMAGE_FROM_DISK);
        assertFalse(NtpCustomizationUtils.shouldAdjustIconTintForNtp(/* isTablet= */ true));

        configManager.setBackgroundImageTypeForTesting(NtpBackgroundImageType.THEME_COLLECTION);
        assertFalse(NtpCustomizationUtils.shouldAdjustIconTintForNtp(/* isTablet= */ true));

        configManager.resetForTesting();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_V2)
    public void testShouldAdjustIconTintForNtp_phone() {
        NtpCustomizationConfigManager configManager = new NtpCustomizationConfigManager();
        NtpCustomizationConfigManager.setInstanceForTesting(configManager);

        configManager.setBackgroundImageTypeForTesting(NtpBackgroundImageType.DEFAULT);
        assertFalse(NtpCustomizationUtils.shouldAdjustIconTintForNtp(/* isTablet= */ false));

        configManager.setBackgroundImageTypeForTesting(NtpBackgroundImageType.CHROME_COLOR);
        assertFalse(NtpCustomizationUtils.shouldAdjustIconTintForNtp(/* isTablet= */ false));

        configManager.setBackgroundImageTypeForTesting(NtpBackgroundImageType.COLOR_FROM_HEX);
        assertFalse(NtpCustomizationUtils.shouldAdjustIconTintForNtp(/* isTablet= */ false));

        configManager.setBackgroundImageTypeForTesting(NtpBackgroundImageType.IMAGE_FROM_DISK);
        assertTrue(NtpCustomizationUtils.shouldAdjustIconTintForNtp(/* isTablet= */ false));

        configManager.setBackgroundImageTypeForTesting(NtpBackgroundImageType.THEME_COLLECTION);
        assertTrue(NtpCustomizationUtils.shouldAdjustIconTintForNtp(/* isTablet= */ false));

        configManager.resetForTesting();
    }

    @Test
    public void testRemoveCustomizedPrimaryColorFromSharedPreference() {
        NtpCustomizationUtils.setCustomizedPrimaryColorToSharedPreference(Color.RED);
        assertEquals(
                Color.RED, NtpCustomizationUtils.getCustomizedPrimaryColorFromSharedPreference());
        NtpCustomizationUtils.removeCustomizedPrimaryColorFromSharedPreference();
        assertEquals(
                NtpThemeColorInfo.COLOR_NOT_SET,
                NtpCustomizationUtils.getCustomizedPrimaryColorFromSharedPreference());
    }

    @Test
    public void testSetAndGetCustomBackgroundInfo() {
        assertNull(NtpCustomizationUtils.getCustomBackgroundInfoFromSharedPreference());

        CustomBackgroundInfo info =
                new CustomBackgroundInfo(JUnitTestGURLs.URL_1, "id", false, true);
        NtpCustomizationUtils.setCustomBackgroundInfoToSharedPreference(info);

        CustomBackgroundInfo restoredInfo =
                NtpCustomizationUtils.getCustomBackgroundInfoFromSharedPreference();
        assertEquals(JUnitTestGURLs.URL_1, restoredInfo.backgroundUrl);
        assertEquals("id", restoredInfo.collectionId);
        assertFalse(restoredInfo.isUploadedImage);
        assertTrue(restoredInfo.isDailyRefreshEnabled);
    }

    @Test
    public void testRemoveCustomBackgroundInfoFromSharedPreference() {
        CustomBackgroundInfo info =
                new CustomBackgroundInfo(JUnitTestGURLs.URL_1, "id", false, true);
        NtpCustomizationUtils.setCustomBackgroundInfoToSharedPreference(info);
        NtpCustomizationUtils.removeCustomBackgroundInfoFromSharedPreference();
        assertNull(NtpCustomizationUtils.getCustomBackgroundInfoFromSharedPreference());
    }

    @Test
    public void testCalculateInitialThemeCollectionImageMatrices() {
        Bitmap bitmap = Bitmap.createBitmap(800, 600, Bitmap.Config.ARGB_8888);
        BackgroundImageInfo info =
                NtpCustomizationUtils.calculateInitialThemeCollectionImageMatrices(
                        mContext, bitmap);
        assertNotNull(info);
        assertNotNull(info.getPortraitMatrix());
        assertNotNull(info.getLandscapeMatrix());
    }

    @Test
    public void testSetAndGetIsChromeColorDailyRefreshEnabledToSharedPreference() {
        ChromeSharedPreferences.getInstance()
                .removeKey(
                        ChromePreferenceKeys.NTP_CUSTOMIZATION_CHROME_COLOR_DAILY_REFRESH_ENABLED);
        assertFalse(
                NtpCustomizationUtils.getIsChromeColorDailyRefreshEnabledFromSharedPreference());

        NtpCustomizationUtils.setIsChromeColorDailyRefreshEnabledToSharedPreference(true);
        assertTrue(NtpCustomizationUtils.getIsChromeColorDailyRefreshEnabledFromSharedPreference());

        NtpCustomizationUtils.setIsChromeColorDailyRefreshEnabledToSharedPreference(false);
        assertFalse(
                NtpCustomizationUtils.getIsChromeColorDailyRefreshEnabledFromSharedPreference());
    }

    @Test
    public void testMaybeUpdateDailyRefreshTimestamp_chromeColor() {
        long timestamp = 100;
        SharedPreferencesManager prefsManager = ChromeSharedPreferences.getInstance();

        // Test case for daily refresh for CHROME_COLOR isn't enabled.
        NtpCustomizationUtils.resetSharedPreferenceForTesting();
        assertFalse(
                NtpCustomizationUtils.getIsChromeColorDailyRefreshEnabledFromSharedPreference());
        NtpCustomizationUtils.maybeUpdateDailyRefreshTimestamp(
                timestamp, NtpBackgroundImageType.CHROME_COLOR, /* customBackgroundInfo= */ null);
        assertFalse(prefsManager.contains(NTP_CUSTOMIZATION_LAST_DAILY_REFRESH_TIMESTAMP));
        assertEquals(0, NtpCustomizationUtils.getDailyRefreshTimestampToSharedPreference());

        // Test case for daily refresh for CHROME_COLOR enabled.
        NtpCustomizationUtils.setIsChromeColorDailyRefreshEnabledToSharedPreference(true);
        NtpCustomizationUtils.maybeUpdateDailyRefreshTimestamp(
                timestamp, NtpBackgroundImageType.CHROME_COLOR, /* customBackgroundInfo= */ null);
        assertEquals(timestamp, NtpCustomizationUtils.getDailyRefreshTimestampToSharedPreference());
    }

    @Test
    public void testMaybeUpdateDailyRefreshTimestamp_themeCollection() {
        long timestamp = 100;
        SharedPreferencesManager prefsManager = ChromeSharedPreferences.getInstance();

        // Test case for daily refresh for THEME_COLLECTION isn't enabled.
        NtpCustomizationUtils.resetSharedPreferenceForTesting();
        CustomBackgroundInfo customBackgroundInfo =
                new CustomBackgroundInfo(JUnitTestGURLs.URL_1, "id", false, false);
        NtpCustomizationUtils.maybeUpdateDailyRefreshTimestamp(
                timestamp, NtpBackgroundImageType.THEME_COLLECTION, customBackgroundInfo);
        assertFalse(prefsManager.contains(NTP_CUSTOMIZATION_LAST_DAILY_REFRESH_TIMESTAMP));

        // Test case for daily refresh for THEME_COLLECTION is enabled.
        customBackgroundInfo = new CustomBackgroundInfo(JUnitTestGURLs.URL_1, "id", false, true);
        NtpCustomizationUtils.maybeUpdateDailyRefreshTimestamp(
                timestamp, NtpBackgroundImageType.THEME_COLLECTION, customBackgroundInfo);
        assertEquals(timestamp, NtpCustomizationUtils.getDailyRefreshTimestampToSharedPreference());
    }

    @Test
    public void testSaveBackgroundInfo() {
        // Scenario 1: With CustomBackgroundInfo, no postponed color picking.
        CustomBackgroundInfo customBackgroundInfo =
                new CustomBackgroundInfo(JUnitTestGURLs.URL_1, "id", false, true);
        Bitmap bitmap = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);
        Matrix portraitMatrix = new Matrix();
        Matrix landscapeMatrix = new Matrix();
        landscapeMatrix.setScale(2.0f, 2.0f);
        BackgroundImageInfo backgroundImageInfo =
                new BackgroundImageInfo(
                        portraitMatrix,
                        landscapeMatrix,
                        /* portraitWindowSize= */ null,
                        /* landscapeWindowSize= */ null);

        NtpCustomizationUtils.saveBackgroundInfo(
                customBackgroundInfo,
                bitmap,
                backgroundImageInfo,
                /* skipSavingPrimaryColor= */ false);
        BaseRobolectricTestRule.runAllBackgroundAndUi(); // Wait for async file operations.

        assertTrue(NtpCustomizationUtils.createBackgroundImageFile().exists());
        CustomBackgroundInfo restoredInfo =
                NtpCustomizationUtils.getCustomBackgroundInfoFromSharedPreference();
        assertEquals(customBackgroundInfo.backgroundUrl, restoredInfo.backgroundUrl);
        assertEquals(customBackgroundInfo.collectionId, restoredInfo.collectionId);
        assertEquals(customBackgroundInfo.isUploadedImage, restoredInfo.isUploadedImage);
        assertEquals(
                customBackgroundInfo.isDailyRefreshEnabled, restoredInfo.isDailyRefreshEnabled);
        assertNotEquals(
                NtpThemeColorInfo.COLOR_NOT_SET,
                NtpCustomizationUtils.getCustomizedPrimaryColorFromSharedPreference());
        BackgroundImageInfo restoredMatrices = NtpCustomizationUtils.readNtpBackgroundImageInfo();
        assertNotNull(restoredMatrices);
        assertEquals(portraitMatrix, restoredMatrices.getPortraitMatrix());
        assertEquals(landscapeMatrix, restoredMatrices.getLandscapeMatrix());

        // Clean up for next scenario.
        NtpCustomizationUtils.deleteBackgroundImageFileImpl(
                NtpCustomizationUtils.createBackgroundImageFile());
        NtpCustomizationUtils.resetSharedPreferenceForTesting();

        // Scenario 2: Without CustomBackgroundInfo, with postponed color picking.
        NtpCustomizationUtils.saveBackgroundInfo(
                /* customBackgroundInfo= */ null,
                bitmap,
                backgroundImageInfo,
                /* skipSavingPrimaryColor= */ true);
        BaseRobolectricTestRule.runAllBackgroundAndUi(); // Wait for async file operations.

        assertTrue(NtpCustomizationUtils.createBackgroundImageFile().exists());
        assertNull(NtpCustomizationUtils.getCustomBackgroundInfoFromSharedPreference());
        assertEquals(
                NtpThemeColorInfo.COLOR_NOT_SET,
                NtpCustomizationUtils.getCustomizedPrimaryColorFromSharedPreference());
        restoredMatrices = NtpCustomizationUtils.readNtpBackgroundImageInfo();
        assertNotNull(restoredMatrices);
        assertEquals(portraitMatrix, restoredMatrices.getPortraitMatrix());
        assertEquals(landscapeMatrix, restoredMatrices.getLandscapeMatrix());
    }

    @Test
    public void testCommitThemeCollectionDailyRefresh() {
        // 1. Set up daily refresh info.
        // BackgroundImageInfo for daily refresh
        Matrix portraitMatrix = new Matrix();
        portraitMatrix.setScale(1.f, 1.f);
        Matrix landscapeMatrix = new Matrix();
        landscapeMatrix.setScale(2.f, 2.f);
        BackgroundImageInfo dailyRefreshImageInfo =
                new BackgroundImageInfo(
                        portraitMatrix, landscapeMatrix, new Point(100, 200), new Point(200, 100));
        NtpCustomizationUtils.updateDailyRefreshBackgroundImageInfo(dailyRefreshImageInfo);

        // Primary color for daily refresh
        int dailyRefreshColor = Color.GREEN;
        NtpCustomizationUtils.setDailyRefreshCustomizedPrimaryColorToSharedPreference(
                dailyRefreshColor);

        // CustomBackgroundInfo for daily refresh
        CustomBackgroundInfo dailyRefreshCustomInfo =
                new CustomBackgroundInfo(
                        JUnitTestGURLs.URL_2,
                        /* collectionId= */ "daily_id",
                        /* isUploadedImage= */ false,
                        /* isDailyRefreshEnabled= */ true);
        NtpCustomizationUtils.setDailyRefreshCustomBackgroundInfoToSharedPreference(
                dailyRefreshCustomInfo);

        // Create daily refresh background image file.
        Bitmap bitmap = Bitmap.createBitmap(1, 1, Bitmap.Config.ARGB_8888);
        File dailyRefreshFile = NtpCustomizationUtils.createDailyRefreshBackgroundImageFile();
        NtpCustomizationUtils.saveBitmapImageToFile(bitmap, dailyRefreshFile);
        BaseRobolectricTestRule.runAllBackgroundAndUi(); // Wait for async file operations.
        assertTrue(dailyRefreshFile.exists());

        // Ensure main file doesn't exist yet, or is different.
        File mainFile = NtpCustomizationUtils.createBackgroundImageFile();
        if (mainFile.exists()) {
            mainFile.delete();
        }
        assertFalse(mainFile.exists());

        // 2. Call the method under test.
        NtpCustomizationUtils.commitThemeCollectionDailyRefresh();
        BaseRobolectricTestRule.runAllBackgroundAndUi(); // Wait for async file operations.

        // 3. Assertions.
        // Check that regular preferences are updated.
        BackgroundImageInfo mainImageInfo = NtpCustomizationUtils.readNtpBackgroundImageInfo();
        assertNotNull(mainImageInfo);
        assertEquals(
                BackgroundImageInfo.matrixToString(portraitMatrix),
                BackgroundImageInfo.matrixToString(mainImageInfo.getPortraitMatrix()));
        assertEquals(
                BackgroundImageInfo.matrixToString(landscapeMatrix),
                BackgroundImageInfo.matrixToString(mainImageInfo.getLandscapeMatrix()));

        assertEquals(
                dailyRefreshColor,
                NtpCustomizationUtils.getCustomizedPrimaryColorFromSharedPreference());

        CustomBackgroundInfo mainCustomInfo =
                NtpCustomizationUtils.getCustomBackgroundInfoFromSharedPreference();
        assertNotNull(mainCustomInfo);
        assertEquals(dailyRefreshCustomInfo.backgroundUrl, mainCustomInfo.backgroundUrl);
        assertEquals(dailyRefreshCustomInfo.collectionId, mainCustomInfo.collectionId);

        // Check that daily refresh preferences are removed.
        assertNull(NtpCustomizationUtils.readDailyRefreshNtpBackgroundImageInfo());
        assertEquals(
                NtpThemeColorInfo.COLOR_NOT_SET,
                NtpCustomizationUtils.getDailyRefreshCustomizedPrimaryColorFromSharedPreference());
        assertNull(NtpCustomizationUtils.getDailyRefreshCustomBackgroundInfoFromSharedPreference());

        // Check file rename.
        assertTrue(mainFile.exists());
        assertFalse(dailyRefreshFile.exists());
    }

    @Test
    public void testSaveAndReadDailyRefreshBackgroundImage() {
        // Saves the bitmap to a file on the disk.
        Bitmap bitmap = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);
        File dailyRefreshFile = NtpCustomizationUtils.createDailyRefreshBackgroundImageFile();
        NtpCustomizationUtils.saveBitmapImageToFile(bitmap, dailyRefreshFile);
        BaseRobolectricTestRule.runAllBackgroundAndUi(); // Wait for async file operations.

        // Reads the bitmap from the file.
        Bitmap bitmapResult = NtpCustomizationUtils.readNtpBackgroundImageImpl(dailyRefreshFile);

        // Verifies that the bitmap read from the file matches the original bitmap.
        assertTrue(bitmap.sameAs(bitmapResult));
    }

    @Test
    public void testSetAndGetDailyRefreshCustomBackgroundInfo() {
        assertNull(NtpCustomizationUtils.getDailyRefreshCustomBackgroundInfoFromSharedPreference());

        CustomBackgroundInfo info =
                new CustomBackgroundInfo(
                        JUnitTestGURLs.URL_1,
                        /* collectionId= */ "daily_id",
                        /* isUploadedImage= */ false,
                        /* isDailyRefreshEnabled= */ true);
        NtpCustomizationUtils.setDailyRefreshCustomBackgroundInfoToSharedPreference(info);

        CustomBackgroundInfo restoredInfo =
                NtpCustomizationUtils.getDailyRefreshCustomBackgroundInfoFromSharedPreference();
        assertEquals(JUnitTestGURLs.URL_1, restoredInfo.backgroundUrl);
        assertEquals("daily_id", restoredInfo.collectionId);
        assertFalse(restoredInfo.isUploadedImage);
        assertTrue(restoredInfo.isDailyRefreshEnabled);

        NtpCustomizationUtils.removeDailyRefreshCustomBackgroundInfoFromSharedPreference();
        assertNull(NtpCustomizationUtils.getDailyRefreshCustomBackgroundInfoFromSharedPreference());
    }

    @Test
    public void testDailyRefreshBackgroundImageInfo() {
        Matrix portrait = new Matrix();
        portrait.setTranslate(10, 10);
        Matrix landscape = new Matrix();
        landscape.setScale(2, 2);

        Point portraitSize = new Point(1080, 1920);
        Point landscapeSize = new Point(2000, 1080);
        BackgroundImageInfo info =
                new BackgroundImageInfo(portrait, landscape, portraitSize, landscapeSize);

        NtpCustomizationUtils.updateDailyRefreshBackgroundImageInfo(info);

        BackgroundImageInfo restoredInfo =
                NtpCustomizationUtils.readDailyRefreshNtpBackgroundImageInfo();
        assertNotNull(restoredInfo);
        assertEquals(info.getPortraitInfoString(), restoredInfo.getPortraitInfoString());
        assertEquals(info.getLandscapeInfoString(), restoredInfo.getLandscapeInfoString());

        NtpCustomizationUtils.removeDailyRefreshNtpBackgroundImageInfo();
        assertNull(NtpCustomizationUtils.readDailyRefreshNtpBackgroundImageInfo());
    }

    @Test
    public void testDailyRefreshCustomizedPrimaryColor() {
        assertEquals(
                NtpThemeColorInfo.COLOR_NOT_SET,
                NtpCustomizationUtils.getDailyRefreshCustomizedPrimaryColorFromSharedPreference());

        int color = Color.RED;
        NtpCustomizationUtils.setDailyRefreshCustomizedPrimaryColorToSharedPreference(color);
        assertEquals(
                color,
                NtpCustomizationUtils.getDailyRefreshCustomizedPrimaryColorFromSharedPreference());

        NtpCustomizationUtils.removeDailyRefreshCustomizedPrimaryColorFromSharedPreference();
        assertEquals(
                NtpThemeColorInfo.COLOR_NOT_SET,
                NtpCustomizationUtils.getDailyRefreshCustomizedPrimaryColorFromSharedPreference());
    }

    @Test
    public void testSaveDailyRefreshBackgroundInfo() {
        // 1. Set up data.
        CustomBackgroundInfo customBackgroundInfo =
                new CustomBackgroundInfo(
                        JUnitTestGURLs.URL_1,
                        /* collectionId= */ "daily_id",
                        /* isUploadedImage= */ false,
                        /* isDailyRefreshEnabled= */ true);
        Bitmap bitmap = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);
        Matrix portraitMatrix = new Matrix();
        Matrix landscapeMatrix = new Matrix();
        landscapeMatrix.setScale(2.0f, 2.0f);
        BackgroundImageInfo backgroundImageInfo =
                new BackgroundImageInfo(
                        portraitMatrix,
                        landscapeMatrix,
                        /* portraitWindowSize= */ null,
                        /* landscapeWindowSize= */ null);

        // 2. Call the method under test.
        NtpCustomizationUtils.saveDailyRefreshBackgroundInfo(
                customBackgroundInfo, bitmap, backgroundImageInfo);
        BaseRobolectricTestRule.runAllBackgroundAndUi(); // Wait for async file operations.

        // 3. Assertions.
        assertTrue(NtpCustomizationUtils.createDailyRefreshBackgroundImageFile().exists());

        CustomBackgroundInfo restoredInfo =
                NtpCustomizationUtils.getDailyRefreshCustomBackgroundInfoFromSharedPreference();
        assertEquals(customBackgroundInfo.backgroundUrl, restoredInfo.backgroundUrl);
        assertEquals(customBackgroundInfo.collectionId, restoredInfo.collectionId);

        assertNotEquals(
                NtpThemeColorInfo.COLOR_NOT_SET,
                NtpCustomizationUtils.getDailyRefreshCustomizedPrimaryColorFromSharedPreference());

        BackgroundImageInfo restoredMatrices =
                NtpCustomizationUtils.readDailyRefreshNtpBackgroundImageInfo();
        assertNotNull(restoredMatrices);
        assertEquals(portraitMatrix, restoredMatrices.getPortraitMatrix());
        assertEquals(landscapeMatrix, restoredMatrices.getLandscapeMatrix());
    }

    @Test
    public void testSetAndGetNtpCustomizationBottomSheetShownFromSharedPreference() {
        assertFalse(
                NtpCustomizationUtils.getNtpCustomizationBottomSheetShownFromSharedPreference());

        NtpCustomizationUtils.setNtpCustomizationBottomSheetShownToSharedPreferences(
                /* hasShown= */ true);
        assertTrue(NtpCustomizationUtils.getNtpCustomizationBottomSheetShownFromSharedPreference());

        NtpCustomizationUtils.setNtpCustomizationBottomSheetShownToSharedPreferences(
                /* hasShown= */ false);
        assertFalse(
                NtpCustomizationUtils.getNtpCustomizationBottomSheetShownFromSharedPreference());
    }
}
