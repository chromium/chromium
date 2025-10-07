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

import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundImageType.IMAGE_FROM_DISK;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.getBackground;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.Matrix;
import android.graphics.drawable.Drawable;

import androidx.annotation.ColorInt;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundImageType;
import org.chromium.chrome.browser.ntp_customization.theme.BackgroundImageInfo;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.ui.shadows.ShadowColorUtils;

import java.io.File;

/** Unit tests for {@link NtpCustomizationUtils} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowColorUtils.class})
public class NtpCustomizationUtilsUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private Tab mTab;
    @Mock private Context mContext;
    @Mock private Drawable mDrawable;

    @After
    public void tearDown() {
        // Clean up preferences to not affect other tests.
        NtpCustomizationUtils.resetSharedPreferenceForTesting();
        ShadowColorUtils.sInNightMode = false;
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
                NtpBackgroundImageType.DEFAULT, NtpCustomizationUtils.getNtpBackgroundImageType());

        @NtpBackgroundImageType int imageType = IMAGE_FROM_DISK;
        NtpCustomizationUtils.setNtpBackgroundImageType(imageType);

        assertEquals(imageType, NtpCustomizationUtils.getNtpBackgroundImageType());
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
                NtpCustomizationConfigManager.COLOR_NOT_SET,
                NtpCustomizationUtils.getCustomizedPrimaryColorFromSharedPreference());

        NtpCustomizationUtils.setCustomizedPrimaryColorToSharedPreference(color);
        assertEquals(color, NtpCustomizationUtils.getCustomizedPrimaryColorFromSharedPreference());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_V2)
    public void testGetPrimaryColorFromCustomizedThemeColor_flagDisabled() {
        assertNull(NtpCustomizationUtils.getPrimaryColorFromCustomizedThemeColor());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_V2)
    public void testGetPrimaryColorFromCustomizedThemeColor_wrongImageType() {
        NtpCustomizationUtils.setNtpBackgroundImageType(NtpBackgroundImageType.DEFAULT);
        NtpCustomizationUtils.setCustomizedPrimaryColorToSharedPreference(Color.RED);
        assertNull(NtpCustomizationUtils.getPrimaryColorFromCustomizedThemeColor());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_V2)
    public void testGetPrimaryColorFromCustomizedThemeColor_colorNotSet() {
        NtpCustomizationUtils.setNtpBackgroundImageType(NtpBackgroundImageType.CHROME_COLOR);
        SharedPreferencesManager prefsManager = ChromeSharedPreferences.getInstance();
        prefsManager.removeKey(ChromePreferenceKeys.NTP_CUSTOMIZATION_PRIMARY_COLOR);

        assertNull(NtpCustomizationUtils.getPrimaryColorFromCustomizedThemeColor());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_V2)
    public void testGetPrimaryColorFromCustomizedThemeColor_colorSet() {
        NtpCustomizationUtils.setNtpBackgroundImageType(NtpBackgroundImageType.CHROME_COLOR);
        NtpCustomizationUtils.setCustomizedPrimaryColorToSharedPreference(Color.RED);

        assertEquals(
                Color.RED, (int) NtpCustomizationUtils.getPrimaryColorFromCustomizedThemeColor());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_V2)
    public void testGetPrimaryColorFromCustomizedThemeColor_colorSetWithImage() {
        NtpCustomizationUtils.setNtpBackgroundImageType(IMAGE_FROM_DISK);
        NtpCustomizationUtils.setCustomizedPrimaryColorToSharedPreference(Color.BLUE);

        assertEquals(
                Color.BLUE, (int) NtpCustomizationUtils.getPrimaryColorFromCustomizedThemeColor());
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
        ShadowColorUtils.sInNightMode = false;
        NtpCustomizationConfigManager customizationConfigManager =
                NtpCustomizationConfigManager.getInstance();

        // Test cases in light mode:

        // Verifies that no tint color is set for the default theme in light mode.
        customizationConfigManager.setBackgroundImageTypeForTesting(NtpBackgroundImageType.DEFAULT);
        NtpCustomizationUtils.setTintForDefaultGoogleLogo(mContext, mDrawable);
        verify(mDrawable, never()).setTint(anyInt());

        // Verifies that color white is set for customized background images.
        customizationConfigManager.setBackgroundImageTypeForTesting(IMAGE_FROM_DISK);
        NtpCustomizationUtils.setTintForDefaultGoogleLogo(mContext, mDrawable);
        verify(mDrawable).setTint(eq(Color.WHITE));

        // Test cases in dark mode:
        ShadowColorUtils.sInNightMode = true;
        clearInvocations(mDrawable);

        // Verifies that color white is set for customized background images.
        customizationConfigManager.setBackgroundImageTypeForTesting(IMAGE_FROM_DISK);
        NtpCustomizationUtils.setTintForDefaultGoogleLogo(mContext, mDrawable);
        verify(mDrawable).setTint(eq(Color.WHITE));

        // Verifies that color white is set for the default theme.
        customizationConfigManager.setBackgroundImageTypeForTesting(NtpBackgroundImageType.DEFAULT);
        NtpCustomizationUtils.setTintForDefaultGoogleLogo(mContext, mDrawable);
        verify(mDrawable, times(2)).setTint(eq(Color.WHITE));

        // Cleans up.
        customizationConfigManager.resetForTesting();
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_V2)
    public void testSetTintForDefaultGoogleLogo_chromeColor() {
        ShadowColorUtils.sInNightMode = false;
        NtpCustomizationConfigManager customizationConfigManager =
                NtpCustomizationConfigManager.getInstance();

        // Test cases in light mode:

        // Verifies that when the primary color is missing, no tint color is set in light mode.
        @ColorInt int primaryColor = Color.RED;
        NtpCustomizationUtils.setNtpBackgroundImageType(NtpBackgroundImageType.CHROME_COLOR);
        customizationConfigManager.setBackgroundImageTypeForTesting(
                NtpBackgroundImageType.CHROME_COLOR);
        assertNull(NtpCustomizationUtils.getPrimaryColorFromCustomizedThemeColor());
        NtpCustomizationUtils.setTintForDefaultGoogleLogo(mContext, mDrawable);
        verify(mDrawable, never()).setTint(anyInt());

        // Verifies that the saved primary color is set for customized color themes if exists.
        NtpCustomizationUtils.setCustomizedPrimaryColorToSharedPreference(primaryColor);
        assertEquals(
                primaryColor,
                NtpCustomizationUtils.getPrimaryColorFromCustomizedThemeColor().intValue());
        NtpCustomizationUtils.setTintForDefaultGoogleLogo(mContext, mDrawable);
        verify(mDrawable).setTint(eq(primaryColor));

        // Test cases in dark mode:
        ShadowColorUtils.sInNightMode = true;
        clearInvocations(mDrawable);

        // Verifies that the saved primary color is set for customized color themes.
        NtpCustomizationUtils.setTintForDefaultGoogleLogo(mContext, mDrawable);
        verify(mDrawable).setTint(eq(primaryColor));

        // Verifies when the primary color is missing, color white is set in dark mode.
        NtpCustomizationUtils.resetSharedPreferenceForTesting();
        assertNull(NtpCustomizationUtils.getPrimaryColorFromCustomizedThemeColor());
        NtpCustomizationUtils.setTintForDefaultGoogleLogo(mContext, mDrawable);
        verify(mDrawable).setTint(eq(Color.WHITE));

        // Cleans up.
        customizationConfigManager.resetForTesting();
    }
}
