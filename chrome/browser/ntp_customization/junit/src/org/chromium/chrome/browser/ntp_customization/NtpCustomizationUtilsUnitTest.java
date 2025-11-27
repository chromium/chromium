// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
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

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.Matrix;
import android.graphics.drawable.Drawable;
import android.os.Build;

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
        mContext = ApplicationProvider.getApplicationContext();
        mContext.setTheme(R.style.Theme_BrowserUI_DayNight);
    }

    @After
    public void tearDown() {
        // Clean up preferences to not affect other tests.
        NtpCustomizationUtils.resetSharedPreferenceForTesting();
        ColorUtils.setInNightModeForTesting(false);
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
        NtpCustomizationUtils.saveBackgroundImageFile(bitmap);

        File file = NtpCustomizationUtils.getBackgroundImageFile();
        assertTrue(file.exists());

        NtpCustomizationUtils.deleteBackgroundImageFileImpl();
        assertFalse(file.exists());
    }

    @Test
    public void testSaveAndReadBackgroundImage() {
        // Saves the bitmap to a file on the disk.
        Bitmap bitmap = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);
        NtpCustomizationUtils.saveBackgroundImageFile(bitmap);

        // Reads the bitmap from the file.
        Bitmap bitmapResult = NtpCustomizationUtils.readNtpBackgroundImageImpl();

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
        assertNull(NtpCustomizationUtils.getPrimaryColorFromCustomizedThemeColor(mContext));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_V2)
    public void testGetPrimaryColorFromCustomizedThemeColor_wrongImageType() {
        NtpCustomizationUtils.setNtpBackgroundImageTypeToSharedPreference(
                NtpBackgroundImageType.DEFAULT);
        NtpCustomizationUtils.setCustomizedPrimaryColorToSharedPreference(Color.RED);
        assertNull(NtpCustomizationUtils.getPrimaryColorFromCustomizedThemeColor(mContext));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_V2)
    public void testGetPrimaryColorFromCustomizedThemeColor_chromeColor_colorNotSet() {
        NtpCustomizationUtils.setNtpBackgroundImageTypeToSharedPreference(
                NtpBackgroundImageType.CHROME_COLOR);
        SharedPreferencesManager prefsManager = ChromeSharedPreferences.getInstance();
        prefsManager.removeKey(ChromePreferenceKeys.NTP_CUSTOMIZATION_PRIMARY_COLOR);

        assertNull(NtpCustomizationUtils.getPrimaryColorFromCustomizedThemeColor(mContext));
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
                (int) NtpCustomizationUtils.getPrimaryColorFromCustomizedThemeColor(mContext));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_V2)
    public void testGetPrimaryColorFromCustomizedThemeColor_colorSetWithImage() {
        NtpCustomizationUtils.setNtpBackgroundImageTypeToSharedPreference(IMAGE_FROM_DISK);
        NtpCustomizationUtils.setCustomizedPrimaryColorToSharedPreference(Color.BLUE);

        assertEquals(
                Color.BLUE,
                (int) NtpCustomizationUtils.getPrimaryColorFromCustomizedThemeColor(mContext));
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
    public void testMatrixToString_identityMatrix_returnsCorrectString() {
        // Creates an identity matrix.
        Matrix matrix = new Matrix();

        String expected = "[1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0]";
        assertEquals(expected, NtpCustomizationUtils.matrixToString(matrix));
    }

    @Test
    public void testMatrixToString_scaledMatrix_returnsCorrectString() {
        Matrix matrix = new Matrix();
        matrix.setScale(2.5f, 3.5f);

        // Verifies if the string for a matrix scaled by (2.5, 3.5)
        String expected = "[2.5, 0.0, 0.0, 0.0, 3.5, 0.0, 0.0, 0.0, 1.0]";
        assertEquals(expected, NtpCustomizationUtils.matrixToString(matrix));
    }

    @Test
    public void testStringToMatrix_validString_returnsCorrectMatrix() {
        String matrixString = "[1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0]";
        Matrix resultMatrix = NtpCustomizationUtils.stringToMatrix(matrixString);

        assertNotNull(resultMatrix);

        Matrix expectedMatrix = new Matrix();
        expectedMatrix.setValues(new float[] {1f, 2f, 3f, 4f, 5f, 6f, 7f, 8f, 9f});

        assertEquals(expectedMatrix, resultMatrix);
    }

    @Test
    public void testStringToMatrix_nullInput_returnsNull() {
        assertNull(NtpCustomizationUtils.stringToMatrix(null));
    }

    @Test
    public void testStringToMatrix_emptyString_returnsNull() {
        assertNull(NtpCustomizationUtils.stringToMatrix(""));
    }

    @Test
    public void testStringToMatrix_malformedString_noBrackets_returnsNull() {
        assertNull(
                NtpCustomizationUtils.stringToMatrix(
                        "1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0"));
    }

    @Test
    public void testStringToMatrix_wrongNumberOfValues_returnsNull() {
        assertNull(NtpCustomizationUtils.stringToMatrix("[1.0, 2.0, 3.0]"));
    }

    @Test
    public void testStringToMatrix_nonFloatValues_returnsNull() {
        assertNull(
                NtpCustomizationUtils.stringToMatrix(
                        "[1.0, abc, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0]"));
    }

    @Test
    public void testReadNtpBackgroundImageMatrices_readsDataWrittenByUpdate() {
        Matrix portraitMatrix = new Matrix();
        Matrix landscapeMatrix = new Matrix();
        landscapeMatrix.setScale(2.5f, 3.5f);

        NtpCustomizationUtils.updateBackgroundImageMatrices(
                new BackgroundImageInfo(portraitMatrix, landscapeMatrix));
        BackgroundImageInfo result = NtpCustomizationUtils.readNtpBackgroundImageMatrices();

        // Verify the read data matches the original written data.
        assertNotNull(result);
        assertEquals(portraitMatrix, result.portraitMatrix);
        assertEquals(landscapeMatrix, result.landscapeMatrix);
    }

    @Test
    public void testSetTintForDefaultGoogleLogo() {
        ColorUtils.setInNightModeForTesting(false);
        NtpCustomizationConfigManager customizationConfigManager =
                new NtpCustomizationConfigManager();
        NtpCustomizationConfigManager.setInstanceForTesting(customizationConfigManager);

        Drawable mutateDrawable = mock(Drawable.class);
        when(mDrawable.mutate()).thenReturn(mutateDrawable);

        // Test cases in light mode:

        // Verifies that no tint color is set for the default theme in light mode.
        customizationConfigManager.setBackgroundImageTypeForTesting(NtpBackgroundImageType.DEFAULT);
        NtpCustomizationUtils.setTintForDefaultGoogleLogo(mContext, mDrawable);
        verify(mutateDrawable, never()).setTint(anyInt());

        // Verifies that color white is set for customized background images.
        customizationConfigManager.setBackgroundImageTypeForTesting(IMAGE_FROM_DISK);
        NtpCustomizationUtils.setTintForDefaultGoogleLogo(mContext, mDrawable);
        verify(mutateDrawable).setTint(eq(Color.WHITE));

        // Test cases in dark mode:
        ColorUtils.setInNightModeForTesting(true);
        clearInvocations(mutateDrawable);

        // Verifies that color white is set for customized background images.
        customizationConfigManager.setBackgroundImageTypeForTesting(IMAGE_FROM_DISK);
        NtpCustomizationUtils.setTintForDefaultGoogleLogo(mContext, mDrawable);
        verify(mutateDrawable).setTint(eq(Color.WHITE));

        // Verifies that color white is set for the default theme.
        customizationConfigManager.setBackgroundImageTypeForTesting(NtpBackgroundImageType.DEFAULT);
        NtpCustomizationUtils.setTintForDefaultGoogleLogo(mContext, mDrawable);
        verify(mutateDrawable, times(2)).setTint(eq(Color.WHITE));

        // Cleans up.
        customizationConfigManager.resetForTesting();
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_V2)
    public void testSetTintForDefaultGoogleLogo_chromeColor() {
        ColorUtils.setInNightModeForTesting(false);
        NtpCustomizationConfigManager customizationConfigManager =
                new NtpCustomizationConfigManager();
        NtpCustomizationConfigManager.setInstanceForTesting(customizationConfigManager);

        Drawable mutateDrawable = mock(Drawable.class);
        when(mDrawable.mutate()).thenReturn(mutateDrawable);

        // Test cases in light mode:

        // Verifies that when the primary color is missing, no tint color is set in light mode.
        @NtpThemeColorId int colorId = NtpThemeColorId.NTP_COLORS_AQUA;
        NtpThemeColorInfo ntpThemeColorInfo =
                NtpThemeColorUtils.createNtpThemeColorInfo(mContext, colorId);
        @ColorInt int primaryColor = mContext.getColor(ntpThemeColorInfo.primaryColorResId);
        NtpCustomizationUtils.setNtpBackgroundImageTypeToSharedPreference(
                NtpBackgroundImageType.CHROME_COLOR);
        customizationConfigManager.setBackgroundImageTypeForTesting(
                NtpBackgroundImageType.CHROME_COLOR);
        assertEquals(
                NtpThemeColorId.DEFAULT,
                NtpCustomizationUtils.getNtpThemeColorIdFromSharedPreference());
        NtpCustomizationUtils.setTintForDefaultGoogleLogo(mContext, mDrawable);
        verify(mutateDrawable, never()).setTint(anyInt());

        // Verifies that the saved primary color is set for customized color themes if exists.
        NtpCustomizationUtils.setNtpThemeColorIdToSharedPreference(colorId);
        assertEquals(colorId, NtpCustomizationUtils.getNtpThemeColorIdFromSharedPreference());
        NtpCustomizationUtils.setTintForDefaultGoogleLogo(mContext, mDrawable);
        verify(mutateDrawable).setTint(eq(primaryColor));

        // Test cases in dark mode:
        ColorUtils.setInNightModeForTesting(true);
        clearInvocations(mutateDrawable);

        // Verifies that the saved primary color is set for customized color themes.
        NtpCustomizationUtils.setTintForDefaultGoogleLogo(mContext, mDrawable);
        verify(mutateDrawable).setTint(eq(primaryColor));

        // Verifies when the primary color is missing, color white is set in dark mode.
        NtpCustomizationUtils.resetSharedPreferenceForTesting();
        assertNull(NtpCustomizationUtils.getPrimaryColorFromCustomizedThemeColor(mContext));
        NtpCustomizationUtils.setTintForDefaultGoogleLogo(mContext, mDrawable);
        verify(mutateDrawable).setTint(eq(Color.WHITE));

        // Cleans up.
        customizationConfigManager.resetForTesting();
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
        assertNotNull(info.portraitMatrix);
        assertNotNull(info.landscapeMatrix);
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
}
