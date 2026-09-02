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

import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.NTP_CUSTOMIZATION_LAST_DAILY_REFRESH_TIMESTAMP;

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
        assertNotEquals(
                NtpThemeColorInfo.COLOR_NOT_SET,
                NtpCustomizationUtils.getCustomizedPrimaryColorFromSharedPreference());

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
                    primaryColor.intValue(),
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
                primaryColor,
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
            assertNotEquals(
                    NtpThemeColorInfo.COLOR_NOT_SET,
                    NtpCustomizationUtils.getCustomizedPrimaryColorFromSharedPreference());
            assertTrue(backgroundData.isBitmapSaved());
        } else {
            assertEquals(
                    primaryColor.intValue(),
                    NtpCustomizationUtils.getCustomizedPrimaryColorFromSharedPreference());
            assertTrue(
                    NtpCustomizationUtils.createThemeCollectionImageFileInDirForTesting(
                                    FILE_ID_HASH)
                            .exists());
            assertTrue(backgroundData.isBitmapSaved());
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

        manager.onSyncedThemeCollectionImageChanged(mContext, themeCollectionData);
        RobolectricUtil.runAllBackgroundAndUi();

        assertEquals(
                NtpBackgroundType.THEME_COLLECTION,
                NtpCustomizationUtils.getNtpBackgroundTypeFromSharedPreference());
        assertEquals(info, NtpCustomizationUtils.getCustomBackgroundInfoFromSharedPreference());
        assertEquals(themeCollectionData, manager.getSyncedNtpBackgroundDataForTesting());
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
        assertEquals(themeCollectionData, manager.getSyncedNtpBackgroundDataForTesting());

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
        assertNull(manager.getSyncedNtpBackgroundDataForTesting());
        verify(mNtpBackgroundDataManager).maybeCleanUpUnusedSyncedImageData(themeCollectionData);
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
}
