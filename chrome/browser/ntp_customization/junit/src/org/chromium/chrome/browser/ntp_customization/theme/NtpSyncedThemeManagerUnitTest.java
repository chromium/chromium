// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.ntp_customization.theme;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

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
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils;
import org.chromium.chrome.browser.ntp_customization.theme.theme_collections.CustomBackgroundInfo;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.image_fetcher.ImageFetcher;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

/** Unit tests for {@link NtpSyncedThemeManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
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
    public void testDestroy() {
        mNtpSyncedThemeManager = new NtpSyncedThemeManager(mContext, mProfile);
        mNtpSyncedThemeManager.destroy();
        verify(mNatives).destroy(anyLong());
    }

    @Test
    public void testOnThemeCollectionSynced() {
        mNtpSyncedThemeManager = new NtpSyncedThemeManager(mContext, mProfile);
        verify(mNatives).init(eq(mProfile), mBridgeCaptor.capture());
        NtpSyncedThemeBridge bridge = mBridgeCaptor.getValue();
        GURL backgroundUrl = JUnitTestGURLs.URL_1;
        CustomBackgroundInfo info =
                new CustomBackgroundInfo(backgroundUrl, "collectionId", false, false);
        when(mNatives.getCustomBackgroundInfo(anyLong())).thenReturn(info);

        bridge.onCustomBackgroundImageUpdated();

        verify(mImageFetcher).fetchImage(any(), mBitmapCallbackCaptor.capture());
        Bitmap bitmap = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);
        mBitmapCallbackCaptor.getValue().onResult(bitmap);
        // Verifying side effects of extractAndSaveSyncedThemeInfo
        CustomBackgroundInfo savedInfo =
                NtpCustomizationUtils.getCustomBackgroundInfoFromSharedPreference();
        assertEquals(info.backgroundUrl, savedInfo.backgroundUrl);
        assertEquals(info.collectionId, savedInfo.collectionId);
        assertEquals(info.isDailyRefreshEnabled, savedInfo.isDailyRefreshEnabled);
        assertEquals(info.isUploadedImage, savedInfo.isUploadedImage);
    }
}
