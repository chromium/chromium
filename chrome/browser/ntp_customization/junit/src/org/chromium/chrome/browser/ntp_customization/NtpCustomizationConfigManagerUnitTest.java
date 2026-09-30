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
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.NTP_BACKGROUND_IMAGE_PORTRAIT_INFO;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.NTP_BACKGROUND_IMAGE_PORTRAIT_INFO_FOR_DAILY_REFRESH;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.NTP_CUSTOMIZATION_BACKGROUND_COLOR;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.NTP_CUSTOMIZATION_BACKGROUND_COLOR_DARK;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.NTP_CUSTOMIZATION_BACKGROUND_IMAGE_FILE_PATH;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.NTP_CUSTOMIZATION_BACKGROUND_INFO;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.NTP_CUSTOMIZATION_BACKGROUND_INFO_FOR_DAILY_REFRESH;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.NTP_CUSTOMIZATION_BACKGROUND_TYPE;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.NTP_CUSTOMIZATION_CHROME_COLOR_DAILY_REFRESH_ENABLED;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.NTP_CUSTOMIZATION_LAST_DAILY_REFRESH_TIMESTAMP;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.NTP_CUSTOMIZATION_PRIMARY_COLOR;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.NTP_CUSTOMIZATION_PRIMARY_COLOR_DARK;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.NTP_CUSTOMIZATION_THEME_COLOR_ID;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.Matrix;
import android.view.ContextThemeWrapper;

import androidx.annotation.ColorInt;
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

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationConfigManager.HomepageStateListener;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundType;
import org.chromium.chrome.browser.ntp_customization.theme.NtpThemeStateProvider;
import org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpThemeColorFromHexInfo;
import org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpThemeColorInfo;
import org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpThemeColorUtils;
import org.chromium.chrome.browser.ntp_customization.theme.daily_refresh.NtpThemeDailyRefreshManager;
import org.chromium.chrome.browser.ntp_customization.theme.theme_collections.CustomBackgroundInfo;
import org.chromium.chrome.browser.ntp_customization.theme.upload_image.BackgroundImageInfo;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.NtpBackgroundDataColor;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.NtpBackgroundDataCustomizedColor;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.NtpBackgroundDataManager;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.NtpBackgroundDataThemeCollection;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.NtpBackgroundDataUploadImage;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.PlatformType;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.ui.theme.ChromeSemanticColorUtils;
import org.chromium.url.JUnitTestGURLs;

import java.io.File;
import java.io.IOException;
import java.util.concurrent.Executor;

/** Unit tests for {@link NtpCustomizationConfigManager}. */
@RunWith(BaseRobolectricTestRunner.class)
public class NtpCustomizationConfigManagerUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private HomepageStateListener mListener;
    @Mock private NtpThemeDailyRefreshManager mNtpThemeDailyRefreshManager;
    @Mock private NtpBackgroundDataManager mNtpBackgroundDataManager;
    @Mock private NtpThemeStateProvider mNtpThemeStateProvider;
    @Captor private ArgumentCaptor<Bitmap> mBitmapCaptor;
    @Captor private ArgumentCaptor<BackgroundImageInfo> mBackgroundImageInfoCaptor;
    @Captor private ArgumentCaptor<Callback<Bitmap>> mBitmapCallbackCaptor;

    private static final String FILE_ID_HASH = "fileIdHash";
    private static final String OTHER_FILE_ID_HASH = "otherFileIdHash";
    private static final String TEST_COLLECTION_ID = "collectionId";
    private static final String OTHER_COLLECTION_ID = "otherCollection";

    private Context mContext;
    private NtpCustomizationConfigManager mNtpCustomizationConfigManager;
    private Matrix mPortraitMatrix;
    private Matrix mLandscapeMatrix;
    private Bitmap mBitmap;
    private BackgroundImageInfo mBackgroundImageInfo;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
        NtpThemeDailyRefreshManager.setInstanceForTesting(mNtpThemeDailyRefreshManager);
        NtpThemeStateProvider.setInstanceForTesting(mNtpThemeStateProvider);
        ThreadUtils.runOnUiThreadBlocking(
                () -> mNtpCustomizationConfigManager = new NtpCustomizationConfigManager());
        mNtpCustomizationConfigManager.setNtpBackgroundDataManagerForTesting(
                mNtpBackgroundDataManager);

        // Makes mPortraitMatrix and mLandscapeMatrix different in terms of values.
        mPortraitMatrix = new Matrix();
        mLandscapeMatrix = new Matrix();
        mPortraitMatrix.setScale(2f, 2f);
        mLandscapeMatrix.setScale(7f, 5f);

        mBitmap = createBitmap();
        mBackgroundImageInfo =
                new BackgroundImageInfo(mPortraitMatrix, mLandscapeMatrix, null, null);
    }

    @After
    public void tearDown() {
        // Clean up listeners to not affect other tests.
        mNtpCustomizationConfigManager.removeListener(mListener);
        mNtpCustomizationConfigManager.resetForTesting();

        // Removes the newly generated file and cleans up SharedPreference.
        NtpCustomizationUtils.resetSharedPreferenceForTesting();
        NtpCustomizationUtils.maybeDeleteFile(NtpCustomizationUtils.createBackgroundImageFile());
        NtpCustomizationUtils.maybeDeleteFile(
                NtpCustomizationUtils.createDailyRefreshBackgroundImageFile());
        NtpCustomizationConfigManager.setInstanceForTesting(null);
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_V2,
        ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_THEME_SYNC
    })
    public void testOnBackgroundDataChanged_Color() {
        mNtpCustomizationConfigManager.addListener(mListener, mContext, /* skipNotify= */ false);
        mNtpCustomizationConfigManager.setBackgroundTypeForTesting(NtpBackgroundType.DEFAULT);
        clearInvocations(mListener);

        // 1. Test non-default color.
        int colorInfoId = NtpThemeColorInfo.NtpThemeColorId.NTP_COLORS_BLUE;
        NtpThemeColorInfo colorInfo =
                NtpThemeColorUtils.createNtpThemeColorInfo(mContext, colorInfoId);
        NtpBackgroundDataColor dataColor =
                new NtpBackgroundDataColor(
                        PlatformType.ANDROID,
                        /* isChromeColorDailyRefreshEnabled= */ false,
                        colorInfo);

        mNtpCustomizationConfigManager.onBackgroundDataChanged(mContext, dataColor);

        // Verify it triggers color change.
        verify(mListener)
                .onBackgroundColorChanged(
                        eq(colorInfo),
                        anyInt(),
                        eq(false),
                        eq(NtpBackgroundType.DEFAULT),
                        eq(NtpBackgroundType.CHROME_COLOR));
        assertEquals(
                NtpBackgroundType.CHROME_COLOR, mNtpCustomizationConfigManager.getBackgroundType());

        clearInvocations(mListener);

        // 2. Test default color.
        NtpThemeColorInfo defaultColorInfo =
                NtpThemeColorUtils.createNtpThemeColorInfo(
                        mContext, NtpThemeColorInfo.NtpThemeColorId.DEFAULT);
        NtpBackgroundDataColor defaultDataColor =
                new NtpBackgroundDataColor(
                        PlatformType.ANDROID,
                        /* isChromeColorDailyRefreshEnabled= */ false,
                        defaultColorInfo);

        mNtpCustomizationConfigManager.onBackgroundDataChanged(mContext, defaultDataColor);

        // Verify it triggers reset.
        verify(mListener).onBackgroundReset(eq(NtpBackgroundType.CHROME_COLOR));
        assertEquals(NtpBackgroundType.DEFAULT, mNtpCustomizationConfigManager.getBackgroundType());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_THEME_SYNC)
    public void testOnUploadedImageSelected_persistsStateAndNotifiesListener() {
        int initialBackgroundType = mNtpCustomizationConfigManager.getBackgroundType();
        mNtpCustomizationConfigManager.addListener(mListener, mContext, /* skipNotify= */ false);

        NtpBackgroundDataUploadImage uploadImageData =
                new NtpBackgroundDataUploadImage(
                        PlatformType.ANDROID,
                        mBackgroundImageInfo,
                        mBitmap,
                        /* primaryColor= */ null,
                        null);
        mNtpCustomizationConfigManager.onBackgroundDataChanged(mContext, uploadImageData);
        RobolectricUtil.runAllBackgroundAndUi();

        assertNotNull(
                ChromeSharedPreferences.getInstance()
                        .readString(
                                ChromePreferenceKeys.NTP_BACKGROUND_IMAGE_PORTRAIT_INFO,
                                /* defaultValue= */ null));
        assertNotNull(
                ChromeSharedPreferences.getInstance()
                        .readString(
                                ChromePreferenceKeys.NTP_BACKGROUND_IMAGE_LANDSCAPE_INFO,
                                /* defaultValue= */ null));
        assertNotNull(NtpCustomizationUtils.getCustomizedPrimaryColorFromSharedPreference());

        // Verifies the listener was notified with the correct parameters.
        verify(mListener)
                .onBackgroundImageChanged(
                        mBitmapCaptor.capture(),
                        mBackgroundImageInfoCaptor.capture(),
                        /* fromInitialization= */ eq(false),
                        /* oldType= */ eq(initialBackgroundType),
                        /* newType= */ eq(NtpBackgroundType.IMAGE_FROM_DISK));

        assertEquals(mBitmap, mBitmapCaptor.getValue());
        assertEquals(mPortraitMatrix, mBackgroundImageInfoCaptor.getValue().getPortraitMatrix());
        assertEquals(mLandscapeMatrix, mBackgroundImageInfoCaptor.getValue().getLandscapeMatrix());
        assertEquals(
                NtpBackgroundType.IMAGE_FROM_DISK,
                mNtpCustomizationConfigManager.getBackgroundType());
        assertEquals(
                NtpBackgroundType.IMAGE_FROM_DISK,
                NtpCustomizationUtils.getNtpBackgroundTypeFromSharedPreference());
        // Verifies that the image file are saved to the disk and matrices are persisted to prefs.
        assertTrue(NtpCustomizationUtils.createBackgroundImageFile().exists());
        assertTrue(uploadImageData.isBitmapSaved());
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_V2,
        ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_THEME_SYNC
    })
    public void testOnUploadedImageSelected_sync() {
        testOnUploadedImageSelectedImpl(/* primaryColor= */ null);
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_V2,
        ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_THEME_SYNC
    })
    public void testOnUploadedImageSelected_fromHistory() {
        testOnUploadedImageSelectedImpl(Color.BLUE);
    }

    private void testOnUploadedImageSelectedImpl(@Nullable @ColorInt Integer primaryColor) {
        NtpBackgroundDataUploadImage uploadImageData =
                new NtpBackgroundDataUploadImage(
                        PlatformType.ANDROID,
                        mBackgroundImageInfo,
                        mBitmap,
                        primaryColor,
                        FILE_ID_HASH);
        mNtpCustomizationConfigManager.onBackgroundDataChanged(mContext, uploadImageData);
        RobolectricUtil.runAllBackgroundAndUi();

        if (primaryColor == null) {
            assertTrue(
                    NtpCustomizationUtils.createUploadImageFileInDirForTesting(FILE_ID_HASH)
                            .exists());
            assertEquals(
                    NtpCustomizationUtils.createUploadImageFileInDirForTesting(FILE_ID_HASH)
                            .getAbsolutePath(),
                    NtpCustomizationUtils.getBackgroundImageFilePathFromSharedPreference());
            assertTrue(uploadImageData.isBitmapSaved());
        } else {
            assertEquals(
                    primaryColor,
                    NtpCustomizationUtils.getCustomizedPrimaryColorFromSharedPreference());
            assertFalse(uploadImageData.isBitmapSaved());
        }
    }

    @Test
    public void testAddListener_skipNotify() {
        mNtpCustomizationConfigManager.setBackgroundTypeForTesting(
                NtpBackgroundType.IMAGE_FROM_DISK);
        // Passes non-null matrices to mNtpCustomizationConfigManager.
        mNtpCustomizationConfigManager.notifyBackgroundImageChanged(
                mBitmap,
                mBackgroundImageInfo,
                /* fromInitialization= */ true,
                /* oldType= */ NtpBackgroundType.DEFAULT);
        mNtpCustomizationConfigManager.setIsInitializedForTesting(true);

        mNtpCustomizationConfigManager.addListener(mListener, mContext, /* skipNotify= */ true);

        // Verifies that the listener isn't notified immediately with skipNotify being true.
        verify(mListener, never())
                .onBackgroundImageChanged(
                        any(Bitmap.class),
                        any(BackgroundImageInfo.class),
                        anyBoolean(),
                        anyInt(),
                        anyInt());
    }

    @Test
    public void testAddListener_notifiesImmediatelyWithImage_forImageFromDisk() {
        mNtpCustomizationConfigManager.setBackgroundTypeForTesting(
                NtpBackgroundType.IMAGE_FROM_DISK);
        // Passes non-null matrices to mNtpCustomizationConfigManager.
        mNtpCustomizationConfigManager.onBackgroundImageChanged(
                mBitmap, mBackgroundImageInfo, /* oldBackgroundType= */ NtpBackgroundType.DEFAULT);
        mNtpCustomizationConfigManager.setIsInitializedForTesting(true);

        mNtpCustomizationConfigManager.addListener(mListener, mContext, /* skipNotify= */ false);

        // Verifies that the listener should be called back immediately with
        // fromInitialization=true.
        verify(mListener)
                .onBackgroundImageChanged(
                        eq(mBitmap),
                        eq(mBackgroundImageInfo),
                        /* fromInitialization= */ eq(true),
                        /* oldType= */ eq(NtpBackgroundType.DEFAULT),
                        /* newType= */ eq(NtpBackgroundType.IMAGE_FROM_DISK));
        verify(mListener, never())
                .onBackgroundColorChanged(any(), anyInt(), anyBoolean(), anyInt(), anyInt());
    }

    @Test
    public void testAddListener_notifiesImmediatelyWithDefaultType() {
        final int defaultColor = NtpThemeColorUtils.getDefaultBackgroundColor(mContext);
        mNtpCustomizationConfigManager.setBackgroundTypeForTesting(NtpBackgroundType.DEFAULT);
        mNtpCustomizationConfigManager.setIsInitializedForTesting(true);

        mNtpCustomizationConfigManager.addListener(mListener, mContext, /* skipNotify= */ false);

        // Verifies that the listener should be notified immediately.
        verify(mListener).onBackgroundReset(/* oldType= */ eq(NtpBackgroundType.DEFAULT));
        verify(mListener, never())
                .onBackgroundImageChanged(any(), any(), anyBoolean(), anyInt(), anyInt());
    }

    @Test
    public void testAddListener_notifiesImmediatelyWithColorFromHex() {
        // Uninitialized COLOR_FROM_HEX without primary color in SharedPreferences resets to
        // DEFAULT.
        mNtpCustomizationConfigManager.setBackgroundTypeForTesting(
                NtpBackgroundType.COLOR_FROM_HEX);
        mNtpCustomizationConfigManager.setIsInitializedForTesting(false);
        NtpCustomizationUtils.removeCustomizedPrimaryColorFromSharedPreference();
        mNtpCustomizationConfigManager.addListener(mListener, mContext, /* skipNotify= */ false);
        assertEquals(NtpBackgroundType.DEFAULT, mNtpCustomizationConfigManager.getBackgroundType());
        verify(mListener, never())
                .onBackgroundColorChanged(any(), anyInt(), anyBoolean(), anyInt(), anyInt());
        verify(mListener).onBackgroundReset(NtpBackgroundType.COLOR_FROM_HEX);
        mNtpCustomizationConfigManager.removeListener(mListener);
        clearInvocations(mListener);

        @ColorInt int primaryColor = Color.RED;
        @ColorInt int backgroundColor = Color.BLUE;
        NtpThemeColorFromHexInfo colorFromHexInfo =
                new NtpThemeColorFromHexInfo(mContext, backgroundColor, primaryColor);
        NtpBackgroundDataCustomizedColor backgroundData =
                new NtpBackgroundDataCustomizedColor(PlatformType.ANDROID, colorFromHexInfo);
        mNtpCustomizationConfigManager.onBackgroundDataChanged(mContext, backgroundData);
        mNtpCustomizationConfigManager.setIsInitializedForTesting(true);

        mNtpCustomizationConfigManager.addListener(mListener, mContext, /* skipNotify= */ false);

        // Verifies that the listener should be called back immediately with
        // fromInitialization=true.
        verify(mListener)
                .onBackgroundColorChanged(
                        eq(colorFromHexInfo),
                        eq(backgroundColor),
                        /* fromInitialization= */ eq(true),
                        /* oldType= */ eq(NtpBackgroundType.DEFAULT),
                        /* newType= */ eq(NtpBackgroundType.COLOR_FROM_HEX));
    }

    @Test
    public void testRemoveListener_stopsReceivingUpdates_onBackgroundChanged() {
        mNtpCustomizationConfigManager.setBackgroundTypeForTesting(
                NtpBackgroundType.IMAGE_FROM_DISK);
        mNtpCustomizationConfigManager.addListener(mListener, mContext, /* skipNotify= */ false);
        mNtpCustomizationConfigManager.removeListener(mListener);

        // Triggers a change that would normally notify the listener.
        clearInvocations(mListener);
        mNtpCustomizationConfigManager.onBackgroundImageChanged(
                mBitmap, mBackgroundImageInfo, NtpBackgroundType.IMAGE_FROM_DISK);

        // Verifies the listener is removed.
        verify(mListener, never())
                .onBackgroundImageChanged(any(), any(), anyBoolean(), anyInt(), anyInt());
    }

    @Test
    public void testRemoveListener_stopsReceivingUpdates_onBackgroundReset() {
        mNtpCustomizationConfigManager.setBackgroundTypeForTesting(NtpBackgroundType.DEFAULT);
        mNtpCustomizationConfigManager.addListener(mListener, mContext, /* skipNotify= */ false);
        mNtpCustomizationConfigManager.removeListener(mListener);

        // Triggers a change that would normally notify the listener.
        clearInvocations(mListener);
        mNtpCustomizationConfigManager.onBackgroundDataChanged(
                mContext, /* backgroundData= */ null);

        // Verifies the listener is removed.
        verify(mListener, never()).onBackgroundReset(anyInt());
    }

    @Test
    public void testAddAndRemoveMvtVisibilityListener() {
        // Verifies the listener added is notified when the visibility if changed.
        mNtpCustomizationConfigManager.addListener(mListener, mContext, /* skipNotify= */ false);
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
        mNtpCustomizationConfigManager.addListener(mListener, mContext, /* skipNotify= */ false);
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
        mNtpCustomizationConfigManager.addListener(mListener, mContext, /* skipNotify= */ false);
        clearInvocations(mListener);

        int colorInfoId = NtpThemeColorInfo.NtpThemeColorId.NTP_COLORS_BLUE;
        NtpThemeColorInfo colorInfo =
                NtpThemeColorUtils.createNtpThemeColorInfo(mContext, colorInfoId);
        NtpBackgroundDataColor backgroundDataColor =
                new NtpBackgroundDataColor(
                        PlatformType.ANDROID,
                        /* isChromeColorDailyRefreshEnabled= */ false,
                        colorInfo);

        @ColorInt
        int backgroundColor =
                NtpThemeColorUtils.getBackgroundColorFromNtpBackgroundData(
                        mContext, backgroundDataColor);
        @ColorInt
        int defaultColor = ChromeSemanticColorUtils.getHomeSurfaceBackgroundColor(mContext);

        assertEquals(defaultColor, NtpThemeColorUtils.getDefaultBackgroundColor(mContext));
        mNtpCustomizationConfigManager.setBackgroundTypeForTesting(NtpBackgroundType.DEFAULT);

        // Test case for choosing a new customized color.
        mNtpCustomizationConfigManager.onBackgroundDataChanged(mContext, backgroundDataColor);
        assertEquals(colorInfoId, NtpCustomizationUtils.getNtpThemeColorIdFromSharedPreference());
        verify(mListener)
                .onBackgroundColorChanged(
                        eq(colorInfo),
                        eq(backgroundColor),
                        eq(false),
                        eq(NtpBackgroundType.DEFAULT),
                        eq(NtpBackgroundType.CHROME_COLOR));

        clearInvocations(mListener);

        // Test case for resetting to the default color.
        mNtpCustomizationConfigManager.onBackgroundDataChanged(
                mContext, /* backgroundData= */ null);
        assertEquals(defaultColor, mNtpCustomizationConfigManager.getBackgroundColor(mContext));
        assertNull(mNtpCustomizationConfigManager.getNtpThemeColorInfo());

        SharedPreferencesManager prefsManager = ChromeSharedPreferences.getInstance();
        assertFalse(prefsManager.contains(ChromePreferenceKeys.NTP_CUSTOMIZATION_BACKGROUND_COLOR));
        assertFalse(prefsManager.contains(ChromePreferenceKeys.NTP_CUSTOMIZATION_THEME_COLOR_ID));
        assertFalse(prefsManager.contains(ChromePreferenceKeys.NTP_CUSTOMIZATION_BACKGROUND_TYPE));
        assertFalse(
                prefsManager.contains(
                        ChromePreferenceKeys.NTP_CUSTOMIZATION_CHROME_COLOR_DAILY_REFRESH_ENABLED));

        verify(mListener).onBackgroundReset(eq(NtpBackgroundType.CHROME_COLOR));
    }

    @Test
    public void testOnBackgroundColorChanged_colorFromHexString() {
        mNtpCustomizationConfigManager.addListener(mListener, mContext, /* skipNotify= */ false);
        clearInvocations(mListener);

        @ColorInt int backgroundColor = Color.RED;
        @ColorInt int primaryColor = Color.BLUE;

        NtpThemeColorFromHexInfo colorFromHexInfo =
                new NtpThemeColorFromHexInfo(mContext, backgroundColor, primaryColor);
        mNtpCustomizationConfigManager.setBackgroundTypeForTesting(NtpBackgroundType.DEFAULT);

        // Test case for choosing a new customized color.
        NtpBackgroundDataCustomizedColor backgroundData =
                new NtpBackgroundDataCustomizedColor(PlatformType.ANDROID, colorFromHexInfo);
        mNtpCustomizationConfigManager.onBackgroundDataChanged(mContext, backgroundData);
        assertEquals(
                backgroundColor,
                NtpCustomizationUtils.getBackgroundColorFromSharedPreference(Color.WHITE));
        assertEquals(
                Integer.valueOf(primaryColor),
                NtpCustomizationUtils.getCustomizedPrimaryColorFromSharedPreference());
        verify(mListener)
                .onBackgroundColorChanged(
                        eq(colorFromHexInfo),
                        eq(backgroundColor),
                        eq(false),
                        eq(NtpBackgroundType.DEFAULT),
                        eq(NtpBackgroundType.COLOR_FROM_HEX));
    }

    @Test
    public void testOnBackgroundColorChanged_dailyRefresh() {
        int colorInfoId = NtpThemeColorInfo.NtpThemeColorId.NTP_COLORS_BLUE;
        NtpThemeColorInfo colorInfo =
                NtpThemeColorUtils.createNtpThemeColorInfo(mContext, colorInfoId);
        mNtpCustomizationConfigManager.setBackgroundTypeForTesting(NtpBackgroundType.DEFAULT);

        // Test case for daily refresh isn't enabled.
        NtpCustomizationUtils.resetSharedPreferenceForTesting();
        assertFalse(
                NtpCustomizationUtils.getIsChromeColorDailyRefreshEnabledFromSharedPreference());

        NtpBackgroundDataColor backgroundData =
                new NtpBackgroundDataColor(
                        PlatformType.ANDROID,
                        /* isChromeColorDailyRefreshEnabled= */ false,
                        colorInfo);
        mNtpCustomizationConfigManager.onBackgroundDataChanged(mContext, backgroundData);
        assertEquals(colorInfoId, NtpCustomizationUtils.getNtpThemeColorIdFromSharedPreference());
        SharedPreferencesManager prefsManager = ChromeSharedPreferences.getInstance();
        assertFalse(prefsManager.contains(NTP_CUSTOMIZATION_LAST_DAILY_REFRESH_TIMESTAMP));

        // Test case for daily refresh enabled.
        NtpCustomizationUtils.setIsChromeColorDailyRefreshEnabledToSharedPreference(true);
        NtpBackgroundDataColor backgroundData2 =
                new NtpBackgroundDataColor(
                        PlatformType.ANDROID,
                        /* isChromeColorDailyRefreshEnabled= */ true,
                        colorInfo);
        mNtpCustomizationConfigManager.onBackgroundDataChanged(mContext, backgroundData2);
        assertEquals(colorInfoId, NtpCustomizationUtils.getNtpThemeColorIdFromSharedPreference());
        assertTrue(prefsManager.contains(NTP_CUSTOMIZATION_LAST_DAILY_REFRESH_TIMESTAMP));
        assertNotEquals(0, NtpCustomizationUtils.getDailyRefreshTimestampToSharedPreference());
    }

    private void testOnBackgroundReset_fromUploadImageImpl(boolean deleteImageFile) {
        if (deleteImageFile) {
            // Re-initialize to pick up enabled features.
            ThreadUtils.runOnUiThreadBlocking(
                    () -> mNtpCustomizationConfigManager = new NtpCustomizationConfigManager());
            mNtpCustomizationConfigManager.setNtpBackgroundDataManagerForTesting(
                    mNtpBackgroundDataManager);
        }
        mNtpCustomizationConfigManager.addListener(mListener, mContext, /* skipNotify= */ false);

        NtpBackgroundDataUploadImage uploadImageData =
                new NtpBackgroundDataUploadImage(
                        PlatformType.ANDROID,
                        mBackgroundImageInfo,
                        mBitmap,
                        /* primaryColor= */ null,
                        FILE_ID_HASH);
        mNtpCustomizationConfigManager.onBackgroundDataChanged(mContext, uploadImageData);
        RobolectricUtil.runAllBackgroundAndUi();
        assertEquals(
                NtpBackgroundType.IMAGE_FROM_DISK,
                mNtpCustomizationConfigManager.getBackgroundType());

        File imageFile = NtpCustomizationUtils.createUploadImageFileInDirForTesting(FILE_ID_HASH);
        assertTrue(imageFile.exists());

        // Test case for resetting to the default color.
        mNtpCustomizationConfigManager.onBackgroundDataChanged(
                mContext, /* backgroundData= */ null);
        RobolectricUtil.runAllBackgroundAndUi();

        assertEquals(NtpBackgroundType.DEFAULT, mNtpCustomizationConfigManager.getBackgroundType());
        assertNull(mNtpCustomizationConfigManager.getBackgroundImageInfoForTesting());
        assertNull(mNtpCustomizationConfigManager.getOriginalBitmapForTesting());
        if (deleteImageFile) {
            assertFalse(imageFile.exists());

        } else {
            assertTrue(imageFile.exists());
        }

        SharedPreferencesManager prefsManager = ChromeSharedPreferences.getInstance();
        assertFalse(prefsManager.contains(ChromePreferenceKeys.NTP_CUSTOMIZATION_BACKGROUND_TYPE));

        verify(mListener).onBackgroundReset(eq(NtpBackgroundType.IMAGE_FROM_DISK));
    }

    @Test
    public void testOnBackgroundReset_fromUploadImage() {
        testOnBackgroundReset_fromUploadImageImpl(/* deleteImageFile= */ true);
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_V2,
        ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_THEME_SYNC
    })
    public void testOnBackgroundReset_fromUploadImage_syncEnabled() {
        testOnBackgroundReset_fromUploadImageImpl(/* deleteImageFile= */ false);
    }

    @Test
    @EnableFeatures({ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_V2})
    public void testOnBackgroundImageLoadedFromDisk_fallback() {
        testOnBackgroundImageLoadedFromDiskImpl(
                /* bitmap= */ null, /* imageInfo= */ null, NtpBackgroundType.DEFAULT);
    }

    @Test
    @EnableFeatures({ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_V2})
    public void testOnBackgroundImageLoadedFromDisk() {
        BackgroundImageInfo imageInfo = mock(BackgroundImageInfo.class);
        testOnBackgroundImageLoadedFromDiskImpl(
                createBitmap(), imageInfo, NtpBackgroundType.IMAGE_FROM_DISK);
    }

    @Test
    public void testOnThemeCollectionImageSelected() {
        int initialBackgroundType = mNtpCustomizationConfigManager.getBackgroundType();
        mNtpCustomizationConfigManager.addListener(mListener, mContext, /* skipNotify= */ false);
        CustomBackgroundInfo customBackgroundInfo =
                new CustomBackgroundInfo(
                        JUnitTestGURLs.NTP_URL,
                        /* collectionId= */ "test",
                        /* isUploadedImage= */ false,
                        /* isDailyRefreshEnabled= */ false);
        NtpBackgroundDataThemeCollection backgroundData =
                new NtpBackgroundDataThemeCollection(
                        PlatformType.ANDROID,
                        customBackgroundInfo,
                        mBackgroundImageInfo,
                        mBitmap,
                        /* primaryColor= */ null,
                        /* fileIdHash= */ null);
        mNtpCustomizationConfigManager.onBackgroundDataChanged(mContext, backgroundData);

        // Verifies the listener was notified with the correct parameters.
        verify(mListener)
                .onBackgroundImageChanged(
                        mBitmapCaptor.capture(),
                        mBackgroundImageInfoCaptor.capture(),
                        /* fromInitialization= */ eq(false),
                        /* oldType= */ eq(initialBackgroundType),
                        /* newType= */ eq(NtpBackgroundType.THEME_COLLECTION));

        assertEquals(mBitmap, mBitmapCaptor.getValue());
        assertEquals(
                mBackgroundImageInfo.getPortraitMatrix(),
                mBackgroundImageInfoCaptor.getValue().getPortraitMatrix());
        assertEquals(
                mBackgroundImageInfo.getLandscapeMatrix(),
                mBackgroundImageInfoCaptor.getValue().getLandscapeMatrix());
        assertEquals(
                NtpBackgroundType.THEME_COLLECTION,
                mNtpCustomizationConfigManager.getBackgroundType());
        assertEquals(
                NtpBackgroundType.THEME_COLLECTION,
                NtpCustomizationUtils.getNtpBackgroundTypeFromSharedPreference());
        assertEquals(
                customBackgroundInfo, mNtpCustomizationConfigManager.getCustomBackgroundInfo());
    }

    @Test
    public void testOnThemeCollectionImageSelected_dailyRefresh() {
        // Test case for daily refresh isn't enabled.
        NtpCustomizationUtils.resetSharedPreferenceForTesting();
        CustomBackgroundInfo customBackgroundInfo =
                new CustomBackgroundInfo(
                        JUnitTestGURLs.NTP_URL,
                        /* collectionId= */ "test",
                        /* isUploadedImage= */ false,
                        /* isDailyRefreshEnabled= */ false);
        NtpBackgroundDataThemeCollection backgroundData =
                new NtpBackgroundDataThemeCollection(
                        PlatformType.ANDROID,
                        customBackgroundInfo,
                        mBackgroundImageInfo,
                        mBitmap,
                        /* primaryColor= */ null,
                        /* fileIdHash= */ null);
        mNtpCustomizationConfigManager.onBackgroundDataChanged(mContext, backgroundData);
        SharedPreferencesManager prefsManager = ChromeSharedPreferences.getInstance();
        assertFalse(prefsManager.contains(NTP_CUSTOMIZATION_LAST_DAILY_REFRESH_TIMESTAMP));

        // Test case for daily refresh enabled.
        customBackgroundInfo =
                new CustomBackgroundInfo(
                        JUnitTestGURLs.NTP_URL,
                        /* collectionId= */ "test",
                        /* isUploadedImage= */ false,
                        /* isDailyRefreshEnabled= */ true);
        backgroundData =
                new NtpBackgroundDataThemeCollection(
                        PlatformType.ANDROID,
                        customBackgroundInfo,
                        mBackgroundImageInfo,
                        mBitmap,
                        /* primaryColor= */ null,
                        /* fileIdHash= */ null);
        mNtpCustomizationConfigManager.onBackgroundDataChanged(mContext, backgroundData);
        assertTrue(prefsManager.contains(NTP_CUSTOMIZATION_LAST_DAILY_REFRESH_TIMESTAMP));
        assertNotEquals(0, NtpCustomizationUtils.getDailyRefreshTimestampToSharedPreference());
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_V2,
        ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_THEME_SYNC
    })
    public void testOnThemeCollectionImageSelected_sync() {
        testOnThemeCollectionImageSelectedImpl(/* primaryColor= */ null);
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_V2,
        ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_THEME_SYNC
    })
    public void testOnThemeCollectionImageSelected_fromHistory() {
        testOnThemeCollectionImageSelectedImpl(Color.RED);
    }

    private void testOnThemeCollectionImageSelectedImpl(@Nullable @ColorInt Integer primaryColor) {
        CustomBackgroundInfo customBackgroundInfo =
                new CustomBackgroundInfo(
                        JUnitTestGURLs.NTP_URL,
                        /* collectionId= */ "test",
                        /* isUploadedImage= */ false,
                        /* isDailyRefreshEnabled= */ false);
        NtpBackgroundDataThemeCollection backgroundData =
                new NtpBackgroundDataThemeCollection(
                        PlatformType.ANDROID,
                        customBackgroundInfo,
                        mBackgroundImageInfo,
                        mBitmap,
                        primaryColor,
                        FILE_ID_HASH);
        mNtpCustomizationConfigManager.onBackgroundDataChanged(mContext, backgroundData);
        RobolectricUtil.runAllBackgroundAndUi();

        if (primaryColor == null) {
            verify(mNtpBackgroundDataManager, never())
                    .saveUserSelectedBackgroundTypeToSharedPreference(any());
            assertTrue(
                    NtpCustomizationUtils.createThemeCollectionImageFileInDirForTesting(
                                    FILE_ID_HASH)
                            .exists());
            assertEquals(
                    NtpCustomizationUtils.createThemeCollectionImageFileInDirForTesting(
                                    FILE_ID_HASH)
                            .getAbsolutePath(),
                    NtpCustomizationUtils.getBackgroundImageFilePathFromSharedPreference());
            assertNotNull(NtpCustomizationUtils.getCustomizedPrimaryColorFromSharedPreference());
            assertTrue(backgroundData.isBitmapSaved());
            // The color extracted from the bitmap must also be kept on the data object, since that
            // is what the outbound sync payload is built from.
            assertEquals(
                    NtpCustomizationUtils.getCustomizedPrimaryColorFromSharedPreference(),
                    backgroundData.getPrimaryColor());
        } else {
            assertEquals(
                    primaryColor,
                    NtpCustomizationUtils.getCustomizedPrimaryColorFromSharedPreference());
            assertTrue(
                    NtpCustomizationUtils.createThemeCollectionImageFileInDirForTesting(
                                    FILE_ID_HASH)
                            .exists());
            assertTrue(backgroundData.isBitmapSaved());
            // An entry that already carries a color keeps it.
            assertEquals(primaryColor, backgroundData.getPrimaryColor());
        }
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_V2,
        ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_THEME_SYNC
    })
    public void testOnThemeCollectionImageSelected_isBitmapSavedTrue() {
        testOnThemeCollectionImageSelected_isBitmapSavedImpl(
                /* isBitmapSaved= */ true, /* expectedSaved= */ false);
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_V2,
        ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_THEME_SYNC
    })
    public void testOnThemeCollectionImageSelected_isBitmapSavedFalse_fromHistory() {
        testOnThemeCollectionImageSelected_isBitmapSavedImpl(
                /* isBitmapSaved= */ false, /* expectedSaved= */ true);
    }

    private void testOnThemeCollectionImageSelected_isBitmapSavedImpl(
            boolean isBitmapSaved, boolean expectedSaved) {
        CustomBackgroundInfo customBackgroundInfo =
                new CustomBackgroundInfo(
                        JUnitTestGURLs.NTP_URL,
                        /* collectionId= */ "test",
                        /* isUploadedImage= */ false,
                        /* isDailyRefreshEnabled= */ false);
        NtpBackgroundDataThemeCollection backgroundData =
                new NtpBackgroundDataThemeCollection(
                        PlatformType.ANDROID,
                        customBackgroundInfo,
                        mBackgroundImageInfo,
                        mBitmap,
                        Color.RED,
                        FILE_ID_HASH);

        File expectedSavedFile =
                NtpCustomizationUtils.createThemeCollectionImageFileInDirForTesting(FILE_ID_HASH);
        if (expectedSavedFile.exists()) {
            expectedSavedFile.delete();
        }
        assertFalse(expectedSavedFile.exists());

        backgroundData.setIsBitmapSaved(isBitmapSaved);

        mNtpCustomizationConfigManager.onBackgroundDataChanged(mContext, backgroundData);
        RobolectricUtil.runAllBackgroundAndUi();

        assertEquals(expectedSaved, expectedSavedFile.exists());
        assertTrue(backgroundData.isBitmapSaved());
    }

    @Test
    public void testAddListener_notifiesImmediatelyWithThemeCollection() {
        mNtpCustomizationConfigManager.setBackgroundTypeForTesting(
                NtpBackgroundType.THEME_COLLECTION);
        // Passes non-null matrices to mNtpCustomizationConfigManager.
        mNtpCustomizationConfigManager.onBackgroundImageChanged(
                mBitmap, mBackgroundImageInfo, NtpBackgroundType.DEFAULT);
        mNtpCustomizationConfigManager.setIsInitializedForTesting(true);

        mNtpCustomizationConfigManager.addListener(mListener, mContext, /* skipNotify= */ false);

        // Verifies that the listener should be called back immediately with
        // fromInitialization=true.
        verify(mListener)
                .onBackgroundImageChanged(
                        eq(mBitmap),
                        eq(mBackgroundImageInfo),
                        /* fromInitialization= */ eq(true),
                        /* oldType= */ eq(NtpBackgroundType.DEFAULT),
                        /* newType= */ eq(NtpBackgroundType.THEME_COLLECTION));
        verify(mListener, never())
                .onBackgroundColorChanged(any(), anyInt(), anyBoolean(), anyInt(), anyInt());
    }

    @Test
    public void testGetCustomBackgroundInfo() {
        mNtpCustomizationConfigManager.setCustomBackgroundInfoForTesting(null);
        assertNull(mNtpCustomizationConfigManager.getCustomBackgroundInfo());

        CustomBackgroundInfo customBackgroundInfo =
                new CustomBackgroundInfo(
                        JUnitTestGURLs.NTP_URL,
                        /* collectionId= */ "test",
                        /* isUploadedImage= */ false,
                        /* isDailyRefreshEnabled= */ false);
        NtpBackgroundDataThemeCollection backgroundData =
                new NtpBackgroundDataThemeCollection(
                        PlatformType.ANDROID,
                        customBackgroundInfo,
                        mBackgroundImageInfo,
                        mBitmap,
                        /* primaryColor= */ null,
                        /* fileIdHash= */ null);
        mNtpCustomizationConfigManager.onBackgroundDataChanged(mContext, backgroundData);
        assertEquals(
                customBackgroundInfo, mNtpCustomizationConfigManager.getCustomBackgroundInfo());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_V2)
    public void testInitialization_withThemeCollection() {
        // 1. Set up shared preferences to indicate a theme collection background.
        NtpCustomizationUtils.setNtpBackgroundTypeToSharedPreference(
                NtpBackgroundType.THEME_COLLECTION);
        CustomBackgroundInfo customBackgroundInfo =
                new CustomBackgroundInfo(
                        JUnitTestGURLs.NTP_URL,
                        /* collectionId= */ "test",
                        /* isUploadedImage= */ false,
                        /* isDailyRefreshEnabled= */ false);

        // 2. Mock NtpThemeDailyRefreshManager behavior.
        when(mNtpThemeDailyRefreshManager.getNtpBackgroundImageInfoForThemeCollection())
                .thenReturn(mBackgroundImageInfo);
        when(mNtpThemeDailyRefreshManager.getNtpCustomBackgroundInfoForThemeCollection())
                .thenReturn(customBackgroundInfo);

        // 3. Create a new instance, which will trigger the constructor logic.
        NtpCustomizationConfigManager configManager =
                ThreadUtils.runOnUiThreadBlocking(NtpCustomizationConfigManager::new);

        // 4. Verify that the manager tried to read the background image.
        verify(mNtpThemeDailyRefreshManager)
                .readNtpBackgroundImageForThemeCollection(
                        mBitmapCallbackCaptor.capture(), any(Executor.class), eq(null));

        // 5. Add a listener and simulate the bitmap becoming available.
        configManager.addListener(mListener, mContext, /* skipNotify= */ true);
        mBitmapCallbackCaptor.getValue().onResult(mBitmap);
        RobolectricUtil.runAllBackgroundAndUi();

        // 6. Verify listener is notified with correct data from initialization.
        verify(mListener)
                .onBackgroundImageChanged(
                        eq(mBitmap),
                        eq(mBackgroundImageInfo),
                        /* fromInitialization= */ eq(true),
                        /* oldType= */ eq(NtpBackgroundType.DEFAULT),
                        /* newType= */ eq(NtpBackgroundType.THEME_COLLECTION));

        // 7. Verify internal state is correct.
        assertEquals(NtpBackgroundType.THEME_COLLECTION, configManager.getBackgroundType());
        assertEquals(customBackgroundInfo, configManager.getCustomBackgroundInfo());
    }

    private void testOnBackgroundImageLoadedFromDiskImpl(
            @Nullable Bitmap bitmap,
            @Nullable BackgroundImageInfo imageInfo,
            @NtpBackgroundType int expectedImageType) {
        mNtpCustomizationConfigManager.setBackgroundTypeForTesting(
                NtpBackgroundType.IMAGE_FROM_DISK);
        assertEquals(
                NtpBackgroundType.IMAGE_FROM_DISK,
                mNtpCustomizationConfigManager.getBackgroundType());
        NtpCustomizationUtils.setNtpBackgroundTypeToSharedPreference(
                NtpBackgroundType.IMAGE_FROM_DISK);
        assertEquals(
                NtpBackgroundType.IMAGE_FROM_DISK,
                NtpCustomizationUtils.getNtpBackgroundTypeFromSharedPreference());

        mNtpCustomizationConfigManager.onBackgroundImageLoadedFromDisk(bitmap, imageInfo);
        assertEquals(expectedImageType, mNtpCustomizationConfigManager.getBackgroundType());
        assertEquals(
                expectedImageType,
                NtpCustomizationUtils.getNtpBackgroundTypeFromSharedPreference());
        assertEquals(imageInfo, mNtpCustomizationConfigManager.getBackgroundImageInfoForTesting());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_V2)
    @DisableFeatures(ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_THEME_SYNC)
    public void testInitialize_imageFromDisk() throws IOException {
        testInitialize_imageFromDiskImpl(/* isSyncEnabled= */ false);
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_V2,
        ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_THEME_SYNC
    })
    public void testInitialize_syncEnabled_imageFromDisk() throws IOException {
        testInitialize_imageFromDiskImpl(/* isSyncEnabled= */ true);
    }

    private void testInitialize_imageFromDiskImpl(boolean isSyncEnabled) throws IOException {
        SharedPreferencesManager prefsManager = ChromeSharedPreferences.getInstance();
        prefsManager.writeInt(
                ChromePreferenceKeys.NTP_CUSTOMIZATION_BACKGROUND_TYPE,
                NtpBackgroundType.IMAGE_FROM_DISK);

        if (isSyncEnabled) {
            File customImageFile =
                    NtpCustomizationUtils.createUploadImageFileInDirForTesting(FILE_ID_HASH);
            NtpCustomizationUtils.saveBitmapImageToFile(mBitmap, customImageFile);
            NtpCustomizationUtils.setBackgroundImageFilePathToSharedPreference(
                    customImageFile.getAbsolutePath());
        } else {
            File defaultImageFile = NtpCustomizationUtils.createBackgroundImageFile();
            NtpCustomizationUtils.saveBitmapImageToFile(mBitmap, defaultImageFile);
        }

        NtpBackgroundDataUploadImage imageData =
                new NtpBackgroundDataUploadImage(
                        PlatformType.ANDROID,
                        mBackgroundImageInfo,
                        mBitmap,
                        /* primaryColor= */ null,
                        isSyncEnabled ? FILE_ID_HASH : null);
        NtpCustomizationUtils.saveBackgroundInfo(imageData, mBitmap, mBackgroundImageInfo);
        assertTrue(imageData.isBitmapSaved());

        NtpCustomizationConfigManager manager =
                ThreadUtils.runOnUiThreadBlocking(NtpCustomizationConfigManager::new);
        manager.setNtpBackgroundDataManagerForTesting(mNtpBackgroundDataManager);

        manager.addListener(mListener, mContext, /* skipNotify= */ false);
        RobolectricUtil.runAllBackgroundAndUi(); // Wait for async read operation.

        verify(mListener)
                .onBackgroundImageChanged(
                        mBitmapCaptor.capture(),
                        any(),
                        /* fromInitialization= */ eq(true),
                        anyInt(),
                        anyInt());

        assertTrue(mBitmap.sameAs(mBitmapCaptor.getValue()));
    }

    private Bitmap createBitmap() {
        return Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_THEME_SYNC)
    public void testOnSyncedThemeCollectionImageChanged() {
        NtpCustomizationConfigManager manager = createConfigManagerWithListener();
        CustomBackgroundInfo info = createTestCustomBackgroundInfo();
        NtpBackgroundDataThemeCollection themeCollectionData = createTestThemeCollectionData(info);

        // A synced Chrome color is pending when the synced theme collection arrives.
        manager.onSyncedChromeColorChanged(mContext, createTestColorData());
        assertTrue(
                ChromeSharedPreferences.getInstance()
                        .contains(ChromePreferenceKeys.NTP_CUSTOMIZATION_THEME_COLOR_ID));

        manager.onSyncedThemeCollectionImageChanged(mContext, themeCollectionData);
        RobolectricUtil.runAllBackgroundAndUi();

        assertEquals(
                NtpBackgroundType.THEME_COLLECTION,
                NtpCustomizationUtils.getNtpBackgroundTypeFromSharedPreference());
        assertEquals(info, NtpCustomizationUtils.getCustomBackgroundInfoFromSharedPreference());
        assertEquals(themeCollectionData, manager.getSyncedNtpBackgroundData());
        assertFalse(
                ChromeSharedPreferences.getInstance()
                        .contains(ChromePreferenceKeys.NTP_CUSTOMIZATION_THEME_COLOR_ID));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_THEME_SYNC)
    public void testMaybeSaveUserSelectedBackgroundTypeToSharedPreference() {
        NtpCustomizationConfigManager manager = createConfigManagerWithListener();
        CustomBackgroundInfo info = createTestCustomBackgroundInfo();
        NtpBackgroundDataThemeCollection themeCollectionData = createTestThemeCollectionData(info);
        manager.setNtpBackgroundDataForTesting(themeCollectionData);

        manager.maybeSaveUserSelectedBackgroundTypeToSharedPreference(mContext);

        verify(mNtpBackgroundDataManager)
                .saveUserSelectedBackgroundTypeToSharedPreference(themeCollectionData);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_THEME_SYNC)
    public void testMaybeApplyBackgroundUpdateFromDeviceSync_withThemeMismatch() {
        NtpCustomizationConfigManager manager = createConfigManagerWithListener();
        CustomBackgroundInfo info = createTestCustomBackgroundInfo();
        NtpBackgroundDataThemeCollection themeCollectionData = createTestThemeCollectionData(info);

        manager.onSyncedThemeCollectionImageChanged(mContext, themeCollectionData);
        RobolectricUtil.runAllBackgroundAndUi();

        verify(mNtpThemeStateProvider, never()).notifyApplyThemeChanges();
        manager.maybeApplyBackgroundUpdateFromDeviceSync(mContext);

        verify(mNtpThemeStateProvider).notifyApplyThemeChanges();
        verify(mNtpBackgroundDataManager)
                .saveUserSelectedBackgroundTypeToSharedPreference(themeCollectionData);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_THEME_SYNC)
    public void testMaybeApplyBackgroundUpdateFromDeviceSync_withoutMismatch() {
        NtpCustomizationConfigManager manager = createConfigManagerWithListener();

        // No synced data has been received.
        verify(mNtpThemeStateProvider, never()).notifyApplyThemeChanges();
        manager.maybeApplyBackgroundUpdateFromDeviceSync(mContext);
        verify(mNtpThemeStateProvider, never()).notifyApplyThemeChanges();
        verify(mNtpBackgroundDataManager, never())
                .saveUserSelectedBackgroundTypeToSharedPreference(any());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_THEME_SYNC)
    public void testMaybeApplyBackgroundUpdateFromDeviceSync_colorOnlyChange_reapplies() {
        NtpCustomizationConfigManager manager = createConfigManagerWithListener();
        CustomBackgroundInfo info = createTestCustomBackgroundInfo();
        manager.onBackgroundDataChanged(
                mContext, createTestThemeCollectionDataWithColor(info, Color.RED));
        RobolectricUtil.runAllBackgroundAndUi();

        // Sync delivers the same image with a different primary color.
        NtpBackgroundDataThemeCollection syncedData =
                createTestThemeCollectionDataWithColor(info, Color.BLUE);
        manager.onSyncedThemeCollectionImageChanged(mContext, syncedData);
        RobolectricUtil.runAllBackgroundAndUi();

        manager.maybeApplyBackgroundUpdateFromDeviceSync(mContext);

        // equals() ignores the color, but the color change must still be applied.
        verify(mNtpThemeStateProvider).notifyApplyThemeChanges();
        verify(mNtpBackgroundDataManager)
                .saveUserSelectedBackgroundTypeToSharedPreference(syncedData);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_THEME_SYNC)
    public void testMaybeApplyBackgroundUpdateFromDeviceSync_sameThemeAndColor_doesNotReapply() {
        NtpCustomizationConfigManager manager = createConfigManagerWithListener();
        CustomBackgroundInfo info = createTestCustomBackgroundInfo();
        manager.onBackgroundDataChanged(
                mContext, createTestThemeCollectionDataWithColor(info, Color.RED));
        RobolectricUtil.runAllBackgroundAndUi();

        // Sync delivers the theme that is already applied.
        manager.onSyncedThemeCollectionImageChanged(
                mContext, createTestThemeCollectionDataWithColor(info, Color.RED));
        RobolectricUtil.runAllBackgroundAndUi();

        manager.maybeApplyBackgroundUpdateFromDeviceSync(mContext);

        verify(mNtpThemeStateProvider, never()).notifyApplyThemeChanges();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_THEME_SYNC)
    public void testClearSyncedNtpBackgroundData() {
        NtpCustomizationConfigManager manager = createConfigManagerWithListener();
        CustomBackgroundInfo info = createTestCustomBackgroundInfo();
        NtpBackgroundDataThemeCollection themeCollectionData = createTestThemeCollectionData(info);

        manager.onSyncedThemeCollectionImageChanged(mContext, themeCollectionData);
        RobolectricUtil.runAllBackgroundAndUi();

        // Clear synced background data before NTP foregrounds.
        manager.clearSyncedNtpBackgroundData(mContext);
        verify(mNtpBackgroundDataManager).maybeCleanUpUnusedSyncedImageData(themeCollectionData);
        verify(mNtpThemeStateProvider, never()).notifyApplyThemeChanges();
        manager.maybeApplyBackgroundUpdateFromDeviceSync(mContext);

        verify(mNtpThemeStateProvider, never()).notifyApplyThemeChanges();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_THEME_SYNC)
    public void testOnSyncedThemeCollectionImageChanged_replacesOldPendingSync_cleansUpOldImage() {
        NtpCustomizationConfigManager manager = createConfigManagerWithListener();
        CustomBackgroundInfo info1 = createTestCustomBackgroundInfo();
        NtpBackgroundDataThemeCollection themeData1 = createTestThemeCollectionData(info1);

        CustomBackgroundInfo info2 =
                new CustomBackgroundInfo(
                        JUnitTestGURLs.URL_2,
                        OTHER_COLLECTION_ID,
                        /* isUploadedImage= */ false,
                        /* isDailyRefreshEnabled= */ false);
        NtpBackgroundDataThemeCollection themeData2 =
                new NtpBackgroundDataThemeCollection(
                        PlatformType.ANDROID,
                        info2,
                        mBackgroundImageInfo,
                        mBitmap,
                        /* primaryColor= */ null,
                        OTHER_FILE_ID_HASH);

        manager.onSyncedThemeCollectionImageChanged(mContext, themeData1);
        RobolectricUtil.runAllBackgroundAndUi();

        // Second sync arrives before first sync was applied.
        manager.onSyncedThemeCollectionImageChanged(mContext, themeData2);
        RobolectricUtil.runAllBackgroundAndUi();

        // Verify old pending sync image was cleaned up.
        verify(mNtpBackgroundDataManager).maybeCleanUpUnusedSyncedImageData(themeData1);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_THEME_SYNC)
    public void testOnBackgroundDataChanged_withPendingSync_clearsPendingSyncAndCleansUp() {
        NtpCustomizationConfigManager manager = createConfigManagerWithListener();
        CustomBackgroundInfo info = createTestCustomBackgroundInfo();
        NtpBackgroundDataThemeCollection themeCollectionData = createTestThemeCollectionData(info);

        manager.onSyncedThemeCollectionImageChanged(mContext, themeCollectionData);
        RobolectricUtil.runAllBackgroundAndUi();
        assertEquals(themeCollectionData, manager.getSyncedNtpBackgroundData());

        // User manually changes background (e.g. to a color or upload image).
        NtpBackgroundDataUploadImage uploadImageData =
                new NtpBackgroundDataUploadImage(
                        PlatformType.ANDROID,
                        mBackgroundImageInfo,
                        mBitmap,
                        /* primaryColor= */ null,
                        FILE_ID_HASH);
        manager.onBackgroundDataChanged(mContext, uploadImageData);
        RobolectricUtil.runAllBackgroundAndUi();

        // Verify pending sync was cleared and its unused image cleaned up.
        assertNull(manager.getSyncedNtpBackgroundData());
        verify(mNtpBackgroundDataManager).maybeCleanUpUnusedSyncedImageData(themeCollectionData);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_THEME_SYNC)
    public void testOnBackgroundDataChanged_selectingTheSyncedTheme_keepsImageAndDoesNotReapply() {
        NtpCustomizationConfigManager manager = createConfigManagerWithListener();
        CustomBackgroundInfo info = createTestCustomBackgroundInfo();
        NtpBackgroundDataThemeCollection themeCollectionData = createTestThemeCollectionData(info);

        manager.onSyncedThemeCollectionImageChanged(mContext, themeCollectionData);
        RobolectricUtil.runAllBackgroundAndUi();
        assertEquals(themeCollectionData, manager.getSyncedNtpBackgroundData());

        // The user applies the very theme sync just delivered. Its primary color has meanwhile
        // been extracted from the bitmap, which no longer makes it a different theme.
        NtpBackgroundDataThemeCollection sameThemeWithColor =
                new NtpBackgroundDataThemeCollection(
                        PlatformType.ANDROID,
                        info,
                        mBackgroundImageInfo,
                        mBitmap,
                        /* primaryColor= */ Color.BLUE,
                        FILE_ID_HASH);
        manager.onBackgroundDataChanged(mContext, sameThemeWithColor);
        RobolectricUtil.runAllBackgroundAndUi();

        // The image is the one being applied, so it must not be cleaned up, and the pending sync
        // state must be dropped so the theme is not applied a second time.
        verify(mNtpBackgroundDataManager, never()).maybeCleanUpUnusedSyncedImageData(any());
        assertNull(manager.getSyncedNtpBackgroundData());

        manager.maybeApplyBackgroundUpdateFromDeviceSync(mContext);
        verify(mNtpThemeStateProvider, never()).notifyApplyThemeChanges();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_THEME_SYNC)
    public void testOnSyncedChromeColorChanged() throws IOException {
        NtpCustomizationConfigManager manager = createConfigManagerWithListener();
        NtpBackgroundDataColor colorData = createTestColorData();
        NtpBackgroundDataThemeCollection themeCollectionData =
                createTestThemeCollectionData(createTestCustomBackgroundInfo());

        // A synced theme collection is pending when the synced Chrome color arrives.
        manager.onSyncedThemeCollectionImageChanged(mContext, themeCollectionData);
        RobolectricUtil.runAllBackgroundAndUi();
        File imageFile = setupCustomizedImageState();

        manager.onSyncedChromeColorChanged(mContext, colorData);
        RobolectricUtil.runAllBackgroundAndUi();

        assertEquals(
                NtpBackgroundType.CHROME_COLOR,
                NtpCustomizationUtils.getNtpBackgroundTypeFromSharedPreference());
        assertEquals(colorData, manager.getSyncedNtpBackgroundData());
        assertCustomizedImageMetadataCleared(imageFile);
        verify(mNtpBackgroundDataManager).maybeCleanUpUnusedSyncedImageData(themeCollectionData);

        manager.maybeApplyBackgroundUpdateFromDeviceSync(mContext);
        RobolectricUtil.runAllBackgroundAndUi();

        verify(mListener)
                .onBackgroundColorChanged(
                        eq(colorData.getNtpThemeColorInfo()),
                        anyInt(),
                        eq(false),
                        eq(NtpBackgroundType.DEFAULT),
                        eq(NtpBackgroundType.CHROME_COLOR));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_THEME_SYNC)
    public void testOnSyncedDefaultThemeReset() throws IOException {
        NtpCustomizationConfigManager manager = createConfigManagerWithListener();
        manager.onBackgroundDataChanged(mContext, createTestColorData());
        RobolectricUtil.runAllBackgroundAndUi();
        clearInvocations(mListener);
        assertTrue(hasPref(NTP_CUSTOMIZATION_THEME_COLOR_ID));

        // A synced theme collection is pending when the synced default theme reset arrives.
        NtpBackgroundDataThemeCollection themeCollectionData =
                createTestThemeCollectionData(createTestCustomBackgroundInfo());
        manager.onSyncedThemeCollectionImageChanged(mContext, themeCollectionData);
        RobolectricUtil.runAllBackgroundAndUi();
        // The active Chrome color data is cleared when the synced theme is persisted.
        assertPrefsRemoved(NTP_CUSTOMIZATION_THEME_COLOR_ID);
        File imageFile = setupCustomizedImageState();

        manager.onSyncedDefaultThemeReset(mContext);
        RobolectricUtil.runAllBackgroundAndUi();

        assertEquals(
                NtpBackgroundType.DEFAULT,
                NtpCustomizationUtils.getNtpBackgroundTypeFromSharedPreference());
        assertNull(manager.getSyncedNtpBackgroundData());
        assertCustomizedImageMetadataCleared(imageFile);
        assertNull(NtpCustomizationUtils.getBackgroundImageFilePathFromSharedPreference());
        verify(mNtpBackgroundDataManager).maybeCleanUpUnusedSyncedImageData(themeCollectionData);
        // The active in-memory state isn't changed until the synced reset is applied.
        assertEquals(NtpBackgroundType.CHROME_COLOR, manager.getBackgroundType());

        manager.maybeApplyBackgroundUpdateFromDeviceSync(mContext);
        RobolectricUtil.runAllBackgroundAndUi();

        verify(mListener).onBackgroundReset(eq(NtpBackgroundType.CHROME_COLOR));
        assertEquals(NtpBackgroundType.DEFAULT, manager.getBackgroundType());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_THEME_SYNC)
    public void testOnSyncedChromeColorChanged_clearsActiveHexColorData() {
        NtpCustomizationConfigManager manager = createConfigManagerWithListener();
        manager.onBackgroundDataChanged(mContext, createTestHexColorData());
        assertHexColorDataPersisted();

        manager.onSyncedChromeColorChanged(mContext, createTestColorData());

        assertHexColorDataCleared();
        assertPersistedBackgroundType(NtpBackgroundType.CHROME_COLOR);
        // The active in-memory state isn't changed until the synced theme is applied.
        assertEquals(NtpBackgroundType.COLOR_FROM_HEX, manager.getBackgroundType());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_THEME_SYNC)
    public void testThemeCollectionToUploadImage_clearsThemeCollectionAndDailyRefreshData() {
        NtpCustomizationConfigManager manager = createConfigManagerWithListener();
        manager.onBackgroundDataChanged(
                mContext, createTestThemeCollectionData(createTestCustomBackgroundInfo()));
        RobolectricUtil.runAllBackgroundAndUi();
        SharedPreferencesManager prefs = ChromeSharedPreferences.getInstance();
        prefs.writeString(NTP_CUSTOMIZATION_BACKGROUND_INFO_FOR_DAILY_REFRESH, "info");
        prefs.writeString(NTP_BACKGROUND_IMAGE_PORTRAIT_INFO_FOR_DAILY_REFRESH, "portrait");
        prefs.writeLong(NTP_CUSTOMIZATION_LAST_DAILY_REFRESH_TIMESTAMP, 100L);

        manager.onBackgroundDataChanged(
                mContext,
                createTestUploadImageData(mBitmap, /* primaryColor= */ null, OTHER_FILE_ID_HASH));
        RobolectricUtil.runAllBackgroundAndUi();

        assertPersistedBackgroundType(NtpBackgroundType.IMAGE_FROM_DISK);
        assertNull(NtpCustomizationUtils.getCustomBackgroundInfoFromSharedPreference());
        assertNull(manager.getCustomBackgroundInfo());
        assertPrefsRemoved(
                NTP_CUSTOMIZATION_BACKGROUND_INFO_FOR_DAILY_REFRESH,
                NTP_BACKGROUND_IMAGE_PORTRAIT_INFO_FOR_DAILY_REFRESH,
                NTP_CUSTOMIZATION_LAST_DAILY_REFRESH_TIMESTAMP);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_THEME_SYNC)
    public void testChromeColorToHexColor_clearsChromeColorData() {
        NtpCustomizationConfigManager manager = createConfigManagerWithListener();
        manager.onBackgroundDataChanged(
                mContext, createTestColorData(/* isDailyRefreshEnabled= */ true));
        assertTrue(hasPref(NTP_CUSTOMIZATION_THEME_COLOR_ID));
        assertTrue(hasPref(NTP_CUSTOMIZATION_CHROME_COLOR_DAILY_REFRESH_ENABLED));

        manager.onBackgroundDataChanged(mContext, createTestHexColorData());

        assertHexColorDataPersisted();
        assertPersistedBackgroundType(NtpBackgroundType.COLOR_FROM_HEX);
        assertPrefsRemoved(
                NTP_CUSTOMIZATION_THEME_COLOR_ID,
                NTP_CUSTOMIZATION_CHROME_COLOR_DAILY_REFRESH_ENABLED);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_THEME_SYNC)
    public void testHexColorToChromeColor_clearsHexColorData() {
        NtpCustomizationConfigManager manager = createConfigManagerWithListener();
        manager.onBackgroundDataChanged(mContext, createTestHexColorData());
        assertHexColorDataPersisted();

        manager.onBackgroundDataChanged(mContext, createTestColorData());

        assertPersistedBackgroundType(NtpBackgroundType.CHROME_COLOR);
        assertHexColorDataCleared();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_THEME_SYNC)
    public void testHexColorToDefault_clearsHexColorData() {
        NtpCustomizationConfigManager manager = createConfigManagerWithListener();
        manager.onBackgroundDataChanged(mContext, createTestHexColorData());
        assertHexColorDataPersisted();

        manager.onBackgroundDataChanged(mContext, /* backgroundData= */ null);

        assertPersistedBackgroundType(NtpBackgroundType.DEFAULT);
        assertHexColorDataCleared();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_THEME_SYNC)
    public void testImageToDefault_clearsFilePath() {
        NtpCustomizationConfigManager manager = createConfigManagerWithListener();
        manager.onBackgroundDataChanged(
                mContext, createTestThemeCollectionData(createTestCustomBackgroundInfo()));
        RobolectricUtil.runAllBackgroundAndUi();
        assertTrue(hasPref(NTP_CUSTOMIZATION_BACKGROUND_IMAGE_FILE_PATH));

        manager.onBackgroundDataChanged(mContext, /* backgroundData= */ null);

        assertPrefsRemoved(NTP_CUSTOMIZATION_BACKGROUND_IMAGE_FILE_PATH);
        assertNull(NtpCustomizationUtils.getCustomBackgroundInfoFromSharedPreference());
        assertNull(manager.getOriginalBitmapForTesting());
        assertNull(manager.getCustomBackgroundInfo());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_THEME_SYNC)
    public void testChromeColorSelected_savesDailyRefreshFromData() {
        NtpCustomizationConfigManager manager = createConfigManagerWithListener();
        NtpBackgroundDataColor colorData = createTestColorData(/* isDailyRefreshEnabled= */ true);

        manager.onBackgroundDataChanged(mContext, colorData);

        assertPersistedChromeColor(colorData);
        assertTrue(NtpCustomizationUtils.getIsChromeColorDailyRefreshEnabledFromSharedPreference());
        assertTrue(hasPref(NTP_CUSTOMIZATION_LAST_DAILY_REFRESH_TIMESTAMP));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_THEME_SYNC)
    public void testChromeColorSelected_dailyRefreshDisabledInData_overridesPref() {
        NtpCustomizationConfigManager manager = createConfigManagerWithListener();
        NtpCustomizationUtils.setIsChromeColorDailyRefreshEnabledToSharedPreference(true);

        manager.onBackgroundDataChanged(mContext, createTestColorData());

        assertFalse(
                NtpCustomizationUtils.getIsChromeColorDailyRefreshEnabledFromSharedPreference());
        assertPrefsRemoved(NTP_CUSTOMIZATION_LAST_DAILY_REFRESH_TIMESTAMP);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_THEME_SYNC)
    public void testUploadImageSelectedFromHistory_persistsTypeAndInfo() {
        NtpCustomizationConfigManager manager = createConfigManagerWithListener();
        manager.onBackgroundDataChanged(mContext, createTestColorData());

        // Upload images in the history list don't carry a bitmap.
        manager.onBackgroundDataChanged(
                mContext, createTestUploadImageData(/* bitmap= */ null, Color.RED, FILE_ID_HASH));

        assertPersistedBackgroundType(NtpBackgroundType.IMAGE_FROM_DISK);
        assertEquals(NtpBackgroundType.IMAGE_FROM_DISK, manager.getBackgroundType());
        assertEquals(
                Integer.valueOf(Color.RED),
                NtpCustomizationUtils.getCustomizedPrimaryColorFromSharedPreference());
        assertTrue(hasPref(NTP_CUSTOMIZATION_BACKGROUND_IMAGE_FILE_PATH));
        assertPrefsRemoved(NTP_CUSTOMIZATION_THEME_COLOR_ID);

        // No mismatch is detected between the in-memory and persisted states.
        manager.maybeApplyBackgroundUpdateFromDeviceSync(mContext);
        assertEquals(NtpBackgroundType.IMAGE_FROM_DISK, manager.getBackgroundType());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_THEME_SYNC)
    public void testOnBackgroundDataChanged_resetWithPendingSyncedThemeCollection_clearsSyncedData()
            throws IOException {
        NtpCustomizationConfigManager manager = createConfigManagerWithListener();
        NtpBackgroundDataThemeCollection themeCollectionData =
                createTestThemeCollectionData(createTestCustomBackgroundInfo());
        manager.onSyncedThemeCollectionImageChanged(mContext, themeCollectionData);
        RobolectricUtil.runAllBackgroundAndUi();
        File imageFile = setupCustomizedImageState();

        // Resets to default before the pending synced theme is applied.
        manager.onBackgroundDataChanged(mContext, /* backgroundData= */ null);
        RobolectricUtil.runAllBackgroundAndUi();

        assertEquals(NtpBackgroundType.DEFAULT, manager.getBackgroundType());
        assertPersistedBackgroundType(NtpBackgroundType.DEFAULT);
        assertNull(manager.getSyncedNtpBackgroundData());
        assertCustomizedImageMetadataCleared(imageFile);
        verify(mNtpBackgroundDataManager).maybeCleanUpUnusedSyncedImageData(themeCollectionData);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_THEME_SYNC)
    public void testOnBackgroundDataChanged_resetWithPendingSyncedChromeColor_clearsSyncedData() {
        NtpCustomizationConfigManager manager = createConfigManagerWithListener();
        manager.onSyncedChromeColorChanged(mContext, createTestColorData());
        assertTrue(hasPref(NTP_CUSTOMIZATION_THEME_COLOR_ID));

        // Resets to default before the pending synced theme is applied.
        manager.onBackgroundDataChanged(mContext, /* backgroundData= */ null);
        RobolectricUtil.runAllBackgroundAndUi();

        assertEquals(NtpBackgroundType.DEFAULT, manager.getBackgroundType());
        assertPersistedBackgroundType(NtpBackgroundType.DEFAULT);
        assertNull(manager.getSyncedNtpBackgroundData());
        assertPrefsRemoved(NTP_CUSTOMIZATION_THEME_COLOR_ID);
        verify(mNtpBackgroundDataManager, never()).maybeCleanUpUnusedSyncedImageData(any());
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_V2,
        ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_THEME_SYNC
    })
    public void testOnBackgroundImageLoadedFromDiskFailed_withPendingSync_keepsSyncedData() {
        NtpCustomizationConfigManager manager = createConfigManagerWithLoadingImage();
        NtpBackgroundDataThemeCollection themeCollectionData =
                createTestThemeCollectionData(createTestCustomBackgroundInfo());
        manager.onSyncedThemeCollectionImageChanged(mContext, themeCollectionData);
        RobolectricUtil.runAllBackgroundAndUi();
        File syncedImageFile =
                NtpCustomizationUtils.createThemeCollectionImageFileInDirForTesting(FILE_ID_HASH);
        assertTrue(syncedImageFile.exists());

        // Failing to load the active background image resets the background, which doesn't go
        // through onBackgroundDataChanged().
        manager.onBackgroundImageLoadedFromDisk(/* bitmap= */ null, /* imageInfo= */ null);
        RobolectricUtil.runAllBackgroundAndUi();

        // The pending synced theme, its persisted data and its image file are kept.
        assertEquals(NtpBackgroundType.DEFAULT, manager.getBackgroundType());
        assertEquals(themeCollectionData, manager.getSyncedNtpBackgroundData());
        assertPersistedBackgroundType(NtpBackgroundType.THEME_COLLECTION);
        assertTrue(syncedImageFile.exists());

        // The pending synced theme is applied later.
        manager.maybeApplyBackgroundUpdateFromDeviceSync(mContext);
        RobolectricUtil.runAllBackgroundAndUi();

        assertEquals(NtpBackgroundType.THEME_COLLECTION, manager.getBackgroundType());
        assertNull(manager.getSyncedNtpBackgroundData());
        assertPersistedBackgroundType(NtpBackgroundType.THEME_COLLECTION);
        assertTrue(syncedImageFile.exists());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_THEME_SYNC)
    public void testOnBackgroundImageLoadedFromDisk_withPendingSyncedChromeColor_keepsSyncedData() {
        NtpCustomizationConfigManager manager = createConfigManagerWithLoadingImage();
        NtpBackgroundDataColor colorData = createTestColorData();
        manager.onSyncedChromeColorChanged(mContext, colorData);

        // The active background image finishes loading after the synced theme is received.
        manager.onBackgroundImageLoadedFromDisk(createBitmap(), mBackgroundImageInfo);

        // The persisted pending synced theme isn't overwritten.
        assertEquals(NtpBackgroundType.IMAGE_FROM_DISK, manager.getBackgroundType());
        assertPersistedChromeColor(colorData);
    }

    @Test
    @DisableFeatures(ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_THEME_SYNC)
    public void testOnBackgroundImageLoadedFromDiskFailed_clearsPrefsAndDeletesFile()
            throws IOException {
        NtpCustomizationConfigManager manager = createConfigManagerWithLoadingImage();
        File imageFile = setupCustomizedImageState();
        NtpCustomizationUtils.setNtpBackgroundTypeToSharedPreference(
                NtpBackgroundType.IMAGE_FROM_DISK);
        NtpCustomizationUtils.setBackgroundImageFilePathToSharedPreference(
                imageFile.getAbsolutePath());

        manager.onBackgroundImageLoadedFromDisk(/* bitmap= */ null, /* imageInfo= */ null);
        RobolectricUtil.runAllBackgroundAndUi();

        assertEquals(NtpBackgroundType.DEFAULT, manager.getBackgroundType());
        assertPrefsRemoved(
                NTP_CUSTOMIZATION_BACKGROUND_TYPE,
                NTP_CUSTOMIZATION_BACKGROUND_IMAGE_FILE_PATH,
                NTP_BACKGROUND_IMAGE_PORTRAIT_INFO,
                NTP_CUSTOMIZATION_BACKGROUND_INFO);
        // The image file is deleted when sync is disabled.
        assertFalse(imageFile.exists());
        verify(mListener).onBackgroundReset(eq(NtpBackgroundType.IMAGE_FROM_DISK));
    }

    /** Creates a manager whose active background image hasn't finished loading from disk. */
    private NtpCustomizationConfigManager createConfigManagerWithLoadingImage() {
        NtpCustomizationConfigManager manager = createConfigManagerWithListener();
        manager.setIsInitializedForTesting(true);
        manager.setBackgroundTypeForTesting(NtpBackgroundType.IMAGE_FROM_DISK);
        return manager;
    }

    private static boolean hasPref(String key) {
        return ChromeSharedPreferences.getInstance().contains(key);
    }

    private static void assertPrefsRemoved(String... keys) {
        for (String key : keys) {
            assertFalse(key, hasPref(key));
        }
    }

    private static void assertPersistedBackgroundType(@NtpBackgroundType int type) {
        assertEquals(type, NtpCustomizationUtils.getNtpBackgroundTypeFromSharedPreference());
    }

    private static void assertPersistedChromeColor(NtpBackgroundDataColor colorData) {
        assertPersistedBackgroundType(NtpBackgroundType.CHROME_COLOR);
        assertEquals(
                colorData.getThemeColorId(),
                NtpCustomizationUtils.getNtpThemeColorIdFromSharedPreference());
    }

    private NtpBackgroundDataCustomizedColor createTestHexColorData() {
        return new NtpBackgroundDataCustomizedColor(
                PlatformType.ANDROID,
                new NtpThemeColorFromHexInfo(mContext, Color.YELLOW, Color.GREEN));
    }

    private static void assertHexColorDataPersisted() {
        assertTrue(hasPref(NTP_CUSTOMIZATION_BACKGROUND_COLOR));
        assertTrue(hasPref(NTP_CUSTOMIZATION_PRIMARY_COLOR));
    }

    private static void assertHexColorDataCleared() {
        assertPrefsRemoved(
                NTP_CUSTOMIZATION_BACKGROUND_COLOR,
                NTP_CUSTOMIZATION_BACKGROUND_COLOR_DARK,
                NTP_CUSTOMIZATION_PRIMARY_COLOR,
                NTP_CUSTOMIZATION_PRIMARY_COLOR_DARK);
    }

    private NtpBackgroundDataColor createTestColorData() {
        return createTestColorData(/* isDailyRefreshEnabled= */ false);
    }

    private NtpBackgroundDataColor createTestColorData(boolean isDailyRefreshEnabled) {
        NtpThemeColorInfo colorInfo =
                NtpThemeColorUtils.createNtpThemeColorInfo(
                        mContext, NtpThemeColorInfo.NtpThemeColorId.NTP_COLORS_BLUE);
        return new NtpBackgroundDataColor(PlatformType.ANDROID, isDailyRefreshEnabled, colorInfo);
    }

    private NtpBackgroundDataUploadImage createTestUploadImageData(
            @Nullable Bitmap bitmap, @Nullable @ColorInt Integer primaryColor, String fileIdHash) {
        return new NtpBackgroundDataUploadImage(
                PlatformType.ANDROID, mBackgroundImageInfo, bitmap, primaryColor, fileIdHash);
    }

    private File setupCustomizedImageState() throws IOException {
        File imageFile = NtpCustomizationUtils.createBackgroundImageFile();
        imageFile.createNewFile();
        assertTrue(imageFile.exists());

        ChromeSharedPreferences.getInstance()
                .writeString(
                        ChromePreferenceKeys.NTP_BACKGROUND_IMAGE_PORTRAIT_INFO, "portrait_info");
        ChromeSharedPreferences.getInstance()
                .writeString(
                        ChromePreferenceKeys.NTP_CUSTOMIZATION_BACKGROUND_INFO, "background_info");
        assertTrue(
                ChromeSharedPreferences.getInstance()
                        .contains(ChromePreferenceKeys.NTP_BACKGROUND_IMAGE_PORTRAIT_INFO));
        assertTrue(
                ChromeSharedPreferences.getInstance()
                        .contains(ChromePreferenceKeys.NTP_CUSTOMIZATION_BACKGROUND_INFO));
        return imageFile;
    }

    private void assertCustomizedImageMetadataCleared(File imageFile) {
        assertFalse(
                ChromeSharedPreferences.getInstance()
                        .contains(ChromePreferenceKeys.NTP_BACKGROUND_IMAGE_PORTRAIT_INFO));
        assertFalse(
                ChromeSharedPreferences.getInstance()
                        .contains(ChromePreferenceKeys.NTP_CUSTOMIZATION_BACKGROUND_INFO));
        // Verifies image file on disk is preserved when sync is enabled.
        assertTrue(imageFile.exists());
    }

    @Test
    public void testMaybeApplyBackgroundUpdateFromDeviceSync_defaultMismatch() {
        NtpCustomizationConfigManager manager = createConfigManagerWithListener();
        int colorId = NtpThemeColorInfo.NtpThemeColorId.NTP_COLORS_BLUE;
        NtpThemeColorInfo colorInfo = NtpThemeColorUtils.createNtpThemeColorInfo(mContext, colorId);
        NtpBackgroundDataColor colorData =
                new NtpBackgroundDataColor(
                        PlatformType.ANDROID,
                        /* isChromeColorDailyRefreshEnabled= */ false,
                        colorInfo);
        manager.onBackgroundDataChanged(mContext, colorData);
        RobolectricUtil.runAllBackgroundAndUi();
        assertEquals(NtpBackgroundType.CHROME_COLOR, manager.getBackgroundType());

        // Now simulate SharedPreference being reset to DEFAULT (e.g. from background sync)
        NtpCustomizationUtils.setNtpBackgroundTypeToSharedPreference(NtpBackgroundType.DEFAULT);

        manager.maybeApplyBackgroundUpdateFromDeviceSync(mContext);
        RobolectricUtil.runAllBackgroundAndUi();

        assertEquals(NtpBackgroundType.DEFAULT, manager.getBackgroundType());
    }

    @Test
    public void testThemeSyncObserver_localAndSyncCommit() {
        NtpCustomizationConfigManager manager =
                ThreadUtils.runOnUiThreadBlocking(NtpCustomizationConfigManager::new);
        manager.setNtpBackgroundDataManagerForTesting(mNtpBackgroundDataManager);
        NtpCustomizationConfigManager.ThemeSyncObserver observer =
                mock(NtpCustomizationConfigManager.ThemeSyncObserver.class);
        manager.addThemeSyncObserver(observer);

        int colorInfoId = NtpThemeColorInfo.NtpThemeColorId.NTP_COLORS_BLUE;
        NtpThemeColorInfo colorInfo =
                NtpThemeColorUtils.createNtpThemeColorInfo(mContext, colorInfoId);
        NtpBackgroundDataColor colorData =
                new NtpBackgroundDataColor(
                        PlatformType.ANDROID,
                        /* isChromeColorDailyRefreshEnabled= */ false,
                        colorInfo);
        manager.onBackgroundDataChanged(
                mContext, colorData, /* shouldNotifyThemeSyncObserver= */ true);
        RobolectricUtil.runAllBackgroundAndUi();
        verify(observer).onThemeCommitted(eq(colorData));

        clearInvocations(observer);
        manager.onBackgroundDataChanged(
                mContext, /* backgroundData= */ null, /* shouldNotifyThemeSyncObserver= */ false);
        RobolectricUtil.runAllBackgroundAndUi();
        verify(observer, never()).onThemeCommitted(any());

        clearInvocations(observer);
        manager.removeThemeSyncObserver(observer);
        manager.onBackgroundDataChanged(
                mContext, colorData, /* shouldNotifyThemeSyncObserver= */ true);
        RobolectricUtil.runAllBackgroundAndUi();
        verify(observer, never()).onThemeCommitted(any());
    }

    private NtpCustomizationConfigManager createConfigManagerWithListener() {
        NtpCustomizationConfigManager manager =
                ThreadUtils.runOnUiThreadBlocking(NtpCustomizationConfigManager::new);
        manager.setNtpBackgroundDataManagerForTesting(mNtpBackgroundDataManager);
        manager.addListener(mListener, mContext, /* skipNotify= */ false);
        RobolectricUtil.runAllBackgroundAndUi();
        return manager;
    }

    private CustomBackgroundInfo createTestCustomBackgroundInfo() {
        return new CustomBackgroundInfo(
                JUnitTestGURLs.URL_1,
                TEST_COLLECTION_ID,
                /* isUploadedImage= */ false,
                /* isDailyRefreshEnabled= */ false);
    }

    private NtpBackgroundDataThemeCollection createTestThemeCollectionData(
            CustomBackgroundInfo info) {
        return new NtpBackgroundDataThemeCollection(
                PlatformType.ANDROID,
                info,
                mBackgroundImageInfo,
                mBitmap,
                /* primaryColor= */ null,
                FILE_ID_HASH);
    }

    private NtpBackgroundDataThemeCollection createTestThemeCollectionDataWithColor(
            CustomBackgroundInfo info, @ColorInt int primaryColor) {
        return new NtpBackgroundDataThemeCollection(
                PlatformType.ANDROID,
                info,
                mBackgroundImageInfo,
                mBitmap,
                primaryColor,
                FILE_ID_HASH);
    }
}
