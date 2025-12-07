// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme.theme_collections;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
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
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationConfigManager;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils;
import org.chromium.chrome.browser.ntp_customization.theme.upload_image.BackgroundImageInfo;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.image_fetcher.ImageFetcher;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.List;

/** Unit tests for {@link NtpThemeCollectionManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class NtpThemeCollectionManagerUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private Profile mProfile;
    @Mock private ImageFetcher mImageFetcher;
    @Mock private NtpThemeCollectionBridge.Natives mNatives;
    @Mock private Runnable mOnThemeImageSelectedCallback;
    @Mock private NtpCustomizationConfigManager mNtpCustomizationConfigManager;

    @Captor private ArgumentCaptor<Callback<Bitmap>> mBitmapCallbackCaptor;

    private NtpThemeCollectionManager mNtpThemeCollectionManager;
    private Context mContext;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        NtpCustomizationUtils.setImageFetcherForTesting(mImageFetcher);
        NtpThemeCollectionBridgeJni.setInstanceForTesting(mNatives);
        when(mNatives.init(any(), any())).thenReturn(1L);
        NtpCustomizationConfigManager.setInstanceForTesting(mNtpCustomizationConfigManager);
        NtpCustomizationUtils.resetSharedPreferenceForTesting();
    }

    @After
    public void tearDown() {
        NtpCustomizationUtils.resetSharedPreferenceForTesting();
    }

    @Test
    public void testDestroy() {
        mNtpThemeCollectionManager =
                new NtpThemeCollectionManager(mContext, mProfile, mOnThemeImageSelectedCallback);
        mNtpThemeCollectionManager.destroy();
        verify(mNatives).destroy(anyLong());
    }

    @Test
    public void testOnCustomBackgroundImageUpdated() {
        mNtpThemeCollectionManager =
                new NtpThemeCollectionManager(mContext, mProfile, mOnThemeImageSelectedCallback);
        GURL backgroundUrl = JUnitTestGURLs.URL_1;
        CustomBackgroundInfo info =
                new CustomBackgroundInfo(backgroundUrl, "collectionId", false, true);
        Bitmap bitmap = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);

        mNtpThemeCollectionManager.onCustomBackgroundImageUpdated(info);

        verify(mImageFetcher).fetchImage(any(), mBitmapCallbackCaptor.capture());
        mBitmapCallbackCaptor.getValue().onResult(bitmap);

        verify(mOnThemeImageSelectedCallback).run();
        verify(mNtpCustomizationConfigManager)
                .onThemeCollectionImageSelected(
                        eq(bitmap), eq(info), any(BackgroundImageInfo.class));
    }

    @Test
    public void testConstructorWithCustomBackground() {
        CustomBackgroundInfo info =
                new CustomBackgroundInfo(JUnitTestGURLs.URL_1, "collection_id", false, true);
        when(mNtpCustomizationConfigManager.getBackgroundImageType())
                .thenReturn(NtpCustomizationUtils.NtpBackgroundImageType.THEME_COLLECTION);
        when(mNtpCustomizationConfigManager.getCustomBackgroundInfo()).thenReturn(info);
        mNtpThemeCollectionManager =
                new NtpThemeCollectionManager(mContext, mProfile, mOnThemeImageSelectedCallback);

        assertEquals("collection_id", mNtpThemeCollectionManager.getSelectedThemeCollectionId());
        assertEquals(
                JUnitTestGURLs.URL_1,
                mNtpThemeCollectionManager.getSelectedThemeCollectionImageUrl());
        assertTrue(mNtpThemeCollectionManager.getIsDailyRefreshEnabled());
    }

    @Test
    public void testResetCustomBackground() {
        mNtpThemeCollectionManager =
                new NtpThemeCollectionManager(mContext, mProfile, mOnThemeImageSelectedCallback);
        mNtpThemeCollectionManager.resetCustomBackground();
        verify(mNatives).resetCustomBackground(anyLong());
    }

    @Test
    public void testSetThemeCollectionImage() {
        mNtpThemeCollectionManager =
                new NtpThemeCollectionManager(mContext, mProfile, mOnThemeImageSelectedCallback);
        CollectionImage image =
                new CollectionImage(
                        "collectionId",
                        JUnitTestGURLs.URL_1,
                        JUnitTestGURLs.URL_2,
                        List.of("attr1", "attr2"),
                        JUnitTestGURLs.URL_3);
        mNtpThemeCollectionManager.setThemeCollectionImage(image);
        verify(mNatives)
                .setThemeCollectionImage(
                        1L,
                        "collectionId",
                        JUnitTestGURLs.URL_1,
                        JUnitTestGURLs.URL_2,
                        "attr1",
                        "attr2",
                        JUnitTestGURLs.URL_3);
    }

    @Test
    public void testSelectLocalBackgroundImage() {
        mNtpThemeCollectionManager =
                new NtpThemeCollectionManager(mContext, mProfile, mOnThemeImageSelectedCallback);
        mNtpThemeCollectionManager.selectLocalBackgroundImage();
        verify(mNatives).selectLocalBackgroundImage(1L);
    }

    @Test
    public void testSetThemeCollectionDailyRefreshed() {
        mNtpThemeCollectionManager =
                new NtpThemeCollectionManager(mContext, mProfile, mOnThemeImageSelectedCallback);
        String collectionId = "test_id";
        mNtpThemeCollectionManager.setThemeCollectionDailyRefreshed(collectionId);
        verify(mNatives).setThemeCollectionDailyRefreshed(eq(1L), eq(collectionId));
    }
}
