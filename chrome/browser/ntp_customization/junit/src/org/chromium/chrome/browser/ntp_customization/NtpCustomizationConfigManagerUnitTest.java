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
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.Matrix;
import android.view.ContextThemeWrapper;

import androidx.annotation.ColorInt;
import androidx.core.content.ContextCompat;
import androidx.test.core.app.ApplicationProvider;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.ThreadUtils;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.test.BaseRobolectricTestRule;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationConfigManager.HomepageStateListener;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundImageType;
import org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpThemeColorFromHexInfo;
import org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpThemeColorInfo;
import org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpThemeColorUtils;
import org.chromium.chrome.browser.ntp_customization.theme.theme_collections.CustomBackgroundInfo;
import org.chromium.chrome.browser.ntp_customization.theme.upload_image.BackgroundImageInfo;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.url.JUnitTestGURLs;

/** Unit tests for {@link NtpCustomizationConfigManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class NtpCustomizationConfigManagerUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private HomepageStateListener mListener;
    @Captor private ArgumentCaptor<Bitmap> mBitmapCaptor;
    @Captor private ArgumentCaptor<BackgroundImageInfo> mBackgroundImageInfoCaptor;

    private Context mContext;
    private NtpCustomizationConfigManager mNtpCustomizationConfigManager;
    private Matrix mPortraitMatrix;
    private Matrix mLandscapeMatrix;
    private Bitmap mBitmap;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
        ThreadUtils.runOnUiThreadBlocking(
                () -> mNtpCustomizationConfigManager = new NtpCustomizationConfigManager());

        // Makes mPortraitMatrix and mLandscapeMatrix different in terms of values.
        mPortraitMatrix = new Matrix();
        mLandscapeMatrix = new Matrix();
        mPortraitMatrix.setScale(2f, 2f);
        mLandscapeMatrix.setScale(7f, 5f);

        mBitmap = createBitmap();
    }

    @After
    public void tearDown() {
        // Clean up listeners to not affect other tests.
        mNtpCustomizationConfigManager.removeListener(mListener);
        mNtpCustomizationConfigManager.resetForTesting();

        // Removes the newly generated file and cleans up SharedPreference.
        NtpCustomizationUtils.resetSharedPreferenceForTesting();
        NtpCustomizationUtils.deleteBackgroundImageFileImpl();
        NtpCustomizationConfigManager.setInstanceForTesting(null);
    }

    @Test
    public void testOnUploadedImageSelected_persistsStateAndNotifiesListener() {
        int initialBackgroundImageType = mNtpCustomizationConfigManager.getBackgroundImageType();
        mNtpCustomizationConfigManager.addListener(mListener, mContext);
        BackgroundImageInfo backgroundImageInfo =
                new BackgroundImageInfo(mPortraitMatrix, mLandscapeMatrix);

        mNtpCustomizationConfigManager.onUploadedImageSelected(mBitmap, backgroundImageInfo);
        BaseRobolectricTestRule.runAllBackgroundAndUi();

        // Verifies that the image file are saved to the disk and matrices are persisted to prefs.
        assertTrue(NtpCustomizationUtils.getBackgroundImageFile().exists());
        assertNotNull(
                ChromeSharedPreferences.getInstance()
                        .readString(
                                ChromePreferenceKeys.NTP_BACKGROUND_IMAGE_PORTRAIT_MATRIX,
                                /* defaultValue= */ null));
        assertNotNull(
                ChromeSharedPreferences.getInstance()
                        .readString(
                                ChromePreferenceKeys.NTP_BACKGROUND_IMAGE_LANDSCAPE_MATRIX,
                                /* defaultValue= */ null));
        assertNotEquals(
                NtpThemeColorInfo.COLOR_NOT_SET,
                NtpCustomizationUtils.getCustomizedPrimaryColorFromSharedPreference());

        // Verifies the listener was notified with the correct parameters.
        verify(mListener)
                .onBackgroundImageChanged(
                        mBitmapCaptor.capture(),
                        mBackgroundImageInfoCaptor.capture(),
                        /* fromInitialization= */ eq(false),
                        /* oldType */ eq(initialBackgroundImageType),
                        /* newType */ eq(NtpBackgroundImageType.IMAGE_FROM_DISK));

        assertEquals(mBitmap, mBitmapCaptor.getValue());
        assertEquals(mPortraitMatrix, mBackgroundImageInfoCaptor.getValue().portraitMatrix);
        assertEquals(mLandscapeMatrix, mBackgroundImageInfoCaptor.getValue().landscapeMatrix);
        assertEquals(
                NtpBackgroundImageType.IMAGE_FROM_DISK,
                mNtpCustomizationConfigManager.getBackgroundImageType());
    }

    @Test
    public void testAddListener_notifiesImmediatelyWithImage_forImageFromDisk() {
        BackgroundImageInfo backgroundImageInfo =
                new BackgroundImageInfo(mPortraitMatrix, mLandscapeMatrix);
        mNtpCustomizationConfigManager.setBackgroundImageTypeForTesting(
                NtpBackgroundImageType.IMAGE_FROM_DISK);
        // Passes non-null matrices to mNtpCustomizationConfigManager.
        mNtpCustomizationConfigManager.notifyBackgroundImageChanged(
                mBitmap,
                backgroundImageInfo,
                /* fromInitialization= */ true,
                /* oldType= */ NtpBackgroundImageType.DEFAULT);
        mNtpCustomizationConfigManager.setIsInitializedForTesting(true);

        mNtpCustomizationConfigManager.addListener(mListener, mContext);

        // Verifies that the listener should be called back immediately with
        // fromInitialization=true.
        verify(mListener)
                .onBackgroundImageChanged(
                        eq(mBitmap),
                        eq(backgroundImageInfo),
                        /* fromInitialization= */ eq(true),
                        /* oldType= */ eq(NtpBackgroundImageType.DEFAULT),
                        /* newType= */ eq(NtpBackgroundImageType.IMAGE_FROM_DISK));
        verify(mListener, never())
                .onBackgroundColorChanged(any(), anyInt(), anyBoolean(), anyInt(), anyInt());
    }

    @Test
    public void testAddListener_notifiesImmediatelyWithDefaultType() {
        final int defaultColor = NtpThemeColorUtils.getDefaultBackgroundColor(mContext);
        mNtpCustomizationConfigManager.setBackgroundImageTypeForTesting(
                NtpBackgroundImageType.DEFAULT);
        mNtpCustomizationConfigManager.setIsInitializedForTesting(true);

        mNtpCustomizationConfigManager.addListener(mListener, mContext);

        // Verifies that the listener should be called back immediately with
        // fromInitialization=true.
        verify(mListener)
                .onBackgroundColorChanged(
                        eq(null),
                        eq(defaultColor),
                        /* fromInitialization= */ eq(true),
                        /* oldType= */ eq(NtpBackgroundImageType.DEFAULT),
                        /* newType= */ eq(NtpBackgroundImageType.DEFAULT));
        verify(mListener, never())
                .onBackgroundImageChanged(any(), any(), anyBoolean(), anyInt(), anyInt());
    }

    @Test
    public void testAddListener_notifiesImmediatelyWithColorFromHex() {
        @ColorInt int primaryColor = Color.RED;
        @ColorInt int backgroundColor = Color.BLUE;
        NtpThemeColorFromHexInfo colorFromHexInfo =
                new NtpThemeColorFromHexInfo(mContext, backgroundColor, primaryColor);
        mNtpCustomizationConfigManager.onBackgroundColorChanged(
                mContext, colorFromHexInfo, NtpBackgroundImageType.COLOR_FROM_HEX);
        mNtpCustomizationConfigManager.setIsInitializedForTesting(true);

        mNtpCustomizationConfigManager.addListener(mListener, mContext);

        // Verifies that the listener should be called back immediately with
        // fromInitialization=true.
        verify(mListener)
                .onBackgroundColorChanged(
                        eq(colorFromHexInfo),
                        eq(backgroundColor),
                        /* fromInitialization= */ eq(true),
                        /* oldType= */ eq(NtpBackgroundImageType.DEFAULT),
                        /* newType= */ eq(NtpBackgroundImageType.COLOR_FROM_HEX));
    }

    @Test
    public void testRemoveListener_stopsReceivingUpdates_onBackgroundChanged() {
        mNtpCustomizationConfigManager.setBackgroundImageTypeForTesting(
                NtpBackgroundImageType.IMAGE_FROM_DISK);
        mNtpCustomizationConfigManager.addListener(mListener, mContext);
        mNtpCustomizationConfigManager.removeListener(mListener);

        // Triggers a change that would normally notify the listener.
        clearInvocations(mListener);
        mNtpCustomizationConfigManager.onBackgroundChanged(
                mBitmap,
                new BackgroundImageInfo(mPortraitMatrix, mLandscapeMatrix),
                NtpBackgroundImageType.IMAGE_FROM_DISK);

        // Verifies the listener is removed.
        verify(mListener, never())
                .onBackgroundImageChanged(any(), any(), anyBoolean(), anyInt(), anyInt());
    }

    @Test
    public void testRemoveListener_stopsReceivingUpdates_onBackgroundColorChanged() {
        mNtpCustomizationConfigManager.setBackgroundImageTypeForTesting(
                NtpBackgroundImageType.DEFAULT);
        mNtpCustomizationConfigManager.addListener(mListener, mContext);
        mNtpCustomizationConfigManager.removeListener(mListener);

        // Triggers a change that would normally notify the listener.
        clearInvocations(mListener);
        mNtpCustomizationConfigManager.onBackgroundColorChanged(
                mContext,
                /* colorInfo= */ null,
                /* backgroundImageType= */ NtpBackgroundImageType.DEFAULT);

        // Verifies the listener is removed.
        verify(mListener, never())
                .onBackgroundColorChanged(any(), anyInt(), anyBoolean(), anyInt(), anyInt());
    }

    @Test
    public void testAddAndRemoveMvtVisibilityListener() {
        // Verifies the listener added is notified when the visibility if changed.
        mNtpCustomizationConfigManager.addListener(mListener, mContext);
        mNtpCustomizationConfigManager.setPrefIsMvtToggleOn(/* isMvtToggleOn= */ true);
        verify(mListener).onMvtToggleChanged();

        // Removes listener and verifies it's not called.
        clearInvocations(mListener);
        mNtpCustomizationConfigManager.removeListener(mListener);
        mNtpCustomizationConfigManager.setPrefIsMvtToggleOn(/* isMvtToggleOn= */ true);
        mNtpCustomizationConfigManager.setPrefIsMvtToggleOn(/* isMvtToggleOn= */ false);
        verify(mListener, never()).onMvtToggleChanged();
    }

    @Test
    public void testSetAndGetPrefMvtVisibility() {
        // Verifies setPrefIsMvtVisible() sets the ChromeSharedPreferences properly and
        // getPrefIsMvtVisible() gets the right value.
        mNtpCustomizationConfigManager.addListener(mListener, mContext);
        mNtpCustomizationConfigManager.setPrefIsMvtToggleOn(/* isMvtToggleOn= */ false);
        assertFalse(
                ChromeSharedPreferences.getInstance()
                        .readBoolean(
                                ChromePreferenceKeys.IS_MVT_VISIBLE, /* defaultValue= */ true));
        assertFalse(mNtpCustomizationConfigManager.getPrefIsMvtToggleOn());
        verify(mListener).onMvtToggleChanged();

        clearInvocations(mListener);
        mNtpCustomizationConfigManager.setPrefIsMvtToggleOn(/* isMvtToggleOn= */ true);
        assertTrue(
                ChromeSharedPreferences.getInstance()
                        .readBoolean(
                                ChromePreferenceKeys.IS_MVT_VISIBLE, /* defaultValue= */ true));
        assertTrue(mNtpCustomizationConfigManager.getPrefIsMvtToggleOn());
        verify(mListener).onMvtToggleChanged();
    }

    @Test
    public void testDefaultPrefMvtVisibility() {
        // Verifies the default value is true.
        assertTrue(mNtpCustomizationConfigManager.getPrefIsMvtToggleOn());
    }

    @Test
    public void testOnBackgroundColorChanged() {
        mNtpCustomizationConfigManager.addListener(mListener, mContext);
        clearInvocations(mListener);

        int colorInfoId = NtpThemeColorInfo.NtpThemeColorId.NTP_COLORS_BLUE;
        NtpThemeColorInfo colorInfo =
                NtpThemeColorUtils.createNtpThemeColorInfo(mContext, colorInfoId);
        @ColorInt
        int backgroundColor =
                NtpThemeColorUtils.getBackgroundColorFromColorInfo(mContext, colorInfo);
        @ColorInt
        int defaultColor = ContextCompat.getColor(mContext, R.color.home_surface_background_color);

        assertEquals(defaultColor, NtpThemeColorUtils.getDefaultBackgroundColor(mContext));
        mNtpCustomizationConfigManager.setBackgroundImageTypeForTesting(
                NtpBackgroundImageType.DEFAULT);

        // Test case for choosing a new customized color.
        mNtpCustomizationConfigManager.onBackgroundColorChanged(
                mContext, colorInfo, NtpBackgroundImageType.CHROME_COLOR);
        assertEquals(colorInfoId, NtpCustomizationUtils.getNtpThemeColorIdFromSharedPreference());
        verify(mListener)
                .onBackgroundColorChanged(
                        eq(colorInfo),
                        eq(backgroundColor),
                        eq(false),
                        eq(NtpBackgroundImageType.DEFAULT),
                        eq(NtpBackgroundImageType.CHROME_COLOR));

        clearInvocations(mListener);
        // Test case for resetting to the default color.
        mNtpCustomizationConfigManager.onBackgroundColorChanged(
                mContext, /* colorInfo= */ null, NtpBackgroundImageType.DEFAULT);
        assertEquals(defaultColor, mNtpCustomizationConfigManager.getBackgroundColor(mContext));

        SharedPreferencesManager prefsManager = ChromeSharedPreferences.getInstance();
        assertFalse(prefsManager.contains(ChromePreferenceKeys.NTP_CUSTOMIZATION_BACKGROUND_COLOR));
        assertFalse(prefsManager.contains(ChromePreferenceKeys.NTP_CUSTOMIZATION_PRIMARY_COLOR));
        assertFalse(prefsManager.contains(ChromePreferenceKeys.NTP_CUSTOMIZATION_THEME_COLOR_ID));

        verify(mListener)
                .onBackgroundColorChanged(
                        eq(null),
                        eq(defaultColor),
                        eq(false),
                        eq(NtpBackgroundImageType.CHROME_COLOR),
                        eq(NtpBackgroundImageType.DEFAULT));
    }

    @Test
    public void testOnBackgroundColorChanged_colorFromHexString() {
        mNtpCustomizationConfigManager.addListener(mListener, mContext);
        clearInvocations(mListener);

        @ColorInt int backgroundColor = Color.RED;
        @ColorInt int primaryColor = Color.BLUE;

        NtpThemeColorFromHexInfo colorFromHexInfo =
                new NtpThemeColorFromHexInfo(mContext, backgroundColor, primaryColor);
        mNtpCustomizationConfigManager.setBackgroundImageTypeForTesting(
                NtpBackgroundImageType.DEFAULT);

        // Test case for choosing a new customized color.
        mNtpCustomizationConfigManager.onBackgroundColorChanged(
                mContext, colorFromHexInfo, NtpBackgroundImageType.COLOR_FROM_HEX);
        assertEquals(
                backgroundColor,
                NtpCustomizationUtils.getBackgroundColorFromSharedPreference(Color.WHITE));
        assertEquals(
                primaryColor,
                NtpCustomizationUtils.getCustomizedPrimaryColorFromSharedPreference());
        verify(mListener)
                .onBackgroundColorChanged(
                        eq(colorFromHexInfo),
                        eq(backgroundColor),
                        eq(false),
                        eq(NtpBackgroundImageType.DEFAULT),
                        eq(NtpBackgroundImageType.COLOR_FROM_HEX));
    }

    @Test
    public void testOnBackgroundImageAvailable_fallback() {
        testOnBackgroundImageAvailableImpl(
                /* bitmap= */ null, /* imageInfo= */ null, NtpBackgroundImageType.DEFAULT);
    }

    @Test
    public void testOnBackgroundImageAvailable() {
        BackgroundImageInfo imageInfo = mock(BackgroundImageInfo.class);
        testOnBackgroundImageAvailableImpl(
                createBitmap(), imageInfo, NtpBackgroundImageType.IMAGE_FROM_DISK);
    }

    @Test
    public void testOnThemeCollectionImageSelected() {
        int initialBackgroundImageType = mNtpCustomizationConfigManager.getBackgroundImageType();
        mNtpCustomizationConfigManager.addListener(mListener, mContext);
        BackgroundImageInfo backgroundImageInfo =
                new BackgroundImageInfo(mPortraitMatrix, mLandscapeMatrix);
        CustomBackgroundInfo customBackgroundInfo =
                new CustomBackgroundInfo(JUnitTestGURLs.NTP_URL, "test", false, false);

        mNtpCustomizationConfigManager.onThemeCollectionImageSelected(
                mBitmap, customBackgroundInfo, backgroundImageInfo);

        // Verifies the listener was notified with the correct parameters.
        verify(mListener)
                .onBackgroundImageChanged(
                        mBitmapCaptor.capture(),
                        mBackgroundImageInfoCaptor.capture(),
                        /* fromInitialization= */ eq(false),
                        /* oldType */ eq(initialBackgroundImageType),
                        /* newType */ eq(NtpBackgroundImageType.THEME_COLLECTION));

        assertEquals(mBitmap, mBitmapCaptor.getValue());
        assertEquals(
                backgroundImageInfo.portraitMatrix,
                mBackgroundImageInfoCaptor.getValue().portraitMatrix);
        assertEquals(
                backgroundImageInfo.landscapeMatrix,
                mBackgroundImageInfoCaptor.getValue().landscapeMatrix);
        assertEquals(
                NtpBackgroundImageType.THEME_COLLECTION,
                mNtpCustomizationConfigManager.getBackgroundImageType());
        assertEquals(
                customBackgroundInfo, mNtpCustomizationConfigManager.getCustomBackgroundInfo());
    }

    @Test
    public void testAddListener_notifiesImmediatelyWithThemeCollection() {
        BackgroundImageInfo backgroundImageInfo =
                new BackgroundImageInfo(mPortraitMatrix, mLandscapeMatrix);
        mNtpCustomizationConfigManager.setBackgroundImageTypeForTesting(
                NtpBackgroundImageType.THEME_COLLECTION);
        // Passes non-null matrices to mNtpCustomizationConfigManager.
        mNtpCustomizationConfigManager.notifyBackgroundImageChanged(
                mBitmap,
                backgroundImageInfo,
                /* fromInitialization= */ true,
                /* oldType= */ NtpBackgroundImageType.DEFAULT);
        mNtpCustomizationConfigManager.setIsInitializedForTesting(true);

        mNtpCustomizationConfigManager.addListener(mListener, mContext);

        // Verifies that the listener should be called back immediately with
        // fromInitialization=true.
        verify(mListener)
                .onBackgroundImageChanged(
                        eq(mBitmap),
                        eq(backgroundImageInfo),
                        /* fromInitialization= */ eq(true),
                        /* oldType= */ eq(NtpBackgroundImageType.DEFAULT),
                        /* newType= */ eq(NtpBackgroundImageType.THEME_COLLECTION));
        verify(mListener, never())
                .onBackgroundColorChanged(any(), anyInt(), anyBoolean(), anyInt(), anyInt());
    }

    @Test
    public void testGetCustomBackgroundInfo() {
        mNtpCustomizationConfigManager.setCustomBackgroundInfoForTesting(null);
        assertNull(mNtpCustomizationConfigManager.getCustomBackgroundInfo());

        CustomBackgroundInfo customBackgroundInfo =
                new CustomBackgroundInfo(JUnitTestGURLs.NTP_URL, "test", false, false);
        mNtpCustomizationConfigManager.onThemeCollectionImageSelected(
                mBitmap,
                customBackgroundInfo,
                new BackgroundImageInfo(mPortraitMatrix, mLandscapeMatrix));
        assertEquals(
                customBackgroundInfo, mNtpCustomizationConfigManager.getCustomBackgroundInfo());
    }

    private void testOnBackgroundImageAvailableImpl(
            @Nullable Bitmap bitmap,
            @Nullable BackgroundImageInfo imageInfo,
            @NtpBackgroundImageType int expectedImageType) {
        mNtpCustomizationConfigManager.setBackgroundImageTypeForTesting(
                NtpBackgroundImageType.IMAGE_FROM_DISK);
        assertEquals(
                NtpBackgroundImageType.IMAGE_FROM_DISK,
                mNtpCustomizationConfigManager.getBackgroundImageType());
        NtpCustomizationUtils.setNtpBackgroundImageTypeToSharedPreference(
                NtpBackgroundImageType.IMAGE_FROM_DISK);
        assertEquals(
                NtpBackgroundImageType.IMAGE_FROM_DISK,
                NtpCustomizationUtils.getNtpBackgroundImageTypeFromSharedPreference());

        mNtpCustomizationConfigManager.onBackgroundImageAvailable(bitmap, imageInfo);
        assertEquals(expectedImageType, mNtpCustomizationConfigManager.getBackgroundImageType());
        assertEquals(
                expectedImageType,
                NtpCustomizationUtils.getNtpBackgroundImageTypeFromSharedPreference());
        assertEquals(imageInfo, mNtpCustomizationConfigManager.getBackgroundImageInfoForTesting());
    }

    private Bitmap createBitmap() {
        return Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);
    }
}
