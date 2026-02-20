// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme;

import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundType.THEME_COLLECTION;

import android.content.Context;
import android.graphics.Bitmap;

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

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.base.test.util.Features;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils;
import org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpThemeColorInfo;
import org.chromium.chrome.browser.ntp_customization.theme.theme_collections.CustomBackgroundInfo;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.image_fetcher.ImageFetcher;
import org.chromium.url.JUnitTestGURLs;

/** Unit tests for {@link NtpSyncedThemeManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Features.EnableFeatures({ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_V2})
public class NtpSyncedThemeManagerUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private Profile mProfile;
    @Mock private ImageFetcher mImageFetcher;
    @Mock private NtpSyncedThemeBridge.Natives mNatives;
    @Captor private ArgumentCaptor<NtpSyncedThemeBridge> mBridgeCaptor;
    @Captor private ArgumentCaptor<Callback<Bitmap>> mBitmapCallbackCaptor;

    private NtpSyncedThemeManager mNtpSyncedThemeManager;
    private Context mContext;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        NtpCustomizationUtils.setImageFetcherForTesting(mImageFetcher);
        NtpSyncedThemeBridgeJni.setInstanceForTesting(mNatives);
        when(mNatives.init(any(), any())).thenReturn(1L);
        NtpCustomizationUtils.resetSharedPreferenceForTesting();
    }

    @After
    public void tearDown() {
        NtpCustomizationUtils.resetSharedPreferenceForTesting();
    }

    @Test
    public void testFetchNextThemeCollectionImageAfterDailyRefreshApplied_dailyRefreshDisabled() {
        NtpCustomizationUtils.setNtpBackgroundTypeToSharedPreference(THEME_COLLECTION);
        CustomBackgroundInfo currentInfo =
                new CustomBackgroundInfo(
                        JUnitTestGURLs.URL_1,
                        "collectionId",
                        /* isUploadedImage= */ false,
                        /* isDailyRefreshEnabled= */ false);
        NtpCustomizationUtils.setCustomBackgroundInfoToSharedPreference(currentInfo);

        mNtpSyncedThemeManager = new NtpSyncedThemeManager(mContext, mProfile);
        mNtpSyncedThemeManager.fetchNextThemeCollectionImageAfterDailyRefreshApplied();
        verify(mNatives, never()).init(any(), any());
    }

    @Test
    public void testFetchNextThemeCollectionImageAfterDailyRefreshApplied_infoAlreadyExists() {
        NtpCustomizationUtils.setNtpBackgroundTypeToSharedPreference(THEME_COLLECTION);
        CustomBackgroundInfo currentInfo =
                new CustomBackgroundInfo(
                        JUnitTestGURLs.URL_1,
                        "collectionId",
                        /* isUploadedImage= */ false,
                        /* isDailyRefreshEnabled= */ true);
        NtpCustomizationUtils.setCustomBackgroundInfoToSharedPreference(currentInfo);

        // Set some daily refresh info.
        CustomBackgroundInfo dailyInfo =
                new CustomBackgroundInfo(
                        JUnitTestGURLs.URL_2,
                        "collectionId",
                        /* isUploadedImage= */ false,
                        /* isDailyRefreshEnabled= */ true);
        NtpCustomizationUtils.setDailyRefreshCustomBackgroundInfoToSharedPreference(dailyInfo);

        mNtpSyncedThemeManager = new NtpSyncedThemeManager(mContext, mProfile);
        mNtpSyncedThemeManager.fetchNextThemeCollectionImageAfterDailyRefreshApplied();
        verify(mNatives, never()).init(any(), any());
    }

    @Test
    public void testFetchNextThemeCollectionImageAfterDailyRefreshApplied() {
        // 1. Set up preconditions.
        NtpCustomizationUtils.setNtpBackgroundTypeToSharedPreference(THEME_COLLECTION);
        CustomBackgroundInfo currentInfo =
                new CustomBackgroundInfo(
                        JUnitTestGURLs.URL_1,
                        "collectionId",
                        /* isUploadedImage= */ false,
                        /* isDailyRefreshEnabled= */ true);
        NtpCustomizationUtils.setCustomBackgroundInfoToSharedPreference(currentInfo);

        // Make sure daily refresh info is not present.
        assertNull(NtpCustomizationUtils.getDailyRefreshCustomBackgroundInfoFromSharedPreference());

        mNtpSyncedThemeManager = new NtpSyncedThemeManager(mContext, mProfile);

        // 2. Call the method.
        mNtpSyncedThemeManager.fetchNextThemeCollectionImageAfterDailyRefreshApplied();

        // 3. Verify bridge is created and fetch is called.
        verify(mNatives).init(eq(mProfile), mBridgeCaptor.capture());
        verify(mNatives).fetchNextThemeCollectionImage(anyLong());

        // 4. Simulate native callback with the info for the next day's image.
        NtpSyncedThemeBridge bridge = mBridgeCaptor.getValue();
        CustomBackgroundInfo nextInfo =
                new CustomBackgroundInfo(
                        JUnitTestGURLs.URL_2,
                        "collectionId",
                        /* isUploadedImage= */ false,
                        /* isDailyRefreshEnabled= */ true);
        when(mNatives.getCustomBackgroundInfo(anyLong())).thenReturn(nextInfo);
        bridge.onCustomBackgroundImageUpdated();

        // 5. Verify image is fetched.
        verify(mImageFetcher).fetchImage(any(), mBitmapCallbackCaptor.capture());
        Bitmap bitmap = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);
        mBitmapCallbackCaptor.getValue().onResult(bitmap);

        RobolectricUtil.runAllBackgroundAndUi();

        // 6. Verify daily refresh info is saved and bridge is destroyed.
        assertTrue(NtpCustomizationUtils.createDailyRefreshBackgroundImageFile().exists());
        assertNotNull(
                NtpCustomizationUtils.getDailyRefreshCustomBackgroundInfoFromSharedPreference());
        assertNotNull(NtpCustomizationUtils.readDailyRefreshNtpBackgroundImageInfo());
        assertNotEquals(
                NtpThemeColorInfo.COLOR_NOT_SET,
                NtpCustomizationUtils.getDailyRefreshCustomizedPrimaryColorFromSharedPreference());

        verify(mNatives).destroy(anyLong());
    }
}
