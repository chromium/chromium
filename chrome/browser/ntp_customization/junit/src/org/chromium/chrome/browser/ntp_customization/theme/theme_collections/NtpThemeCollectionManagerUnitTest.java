// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme.theme_collections;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.graphics.Bitmap;

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
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationConfigManager;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.NtpBackgroundDataThemeCollection;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.image_fetcher.ImageFetcher;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.List;

/** Unit tests for {@link NtpThemeCollectionManager}. */
@RunWith(BaseRobolectricTestRunner.class)
public class NtpThemeCollectionManagerUnitTest {
    public static final long NATIVE_NTP_THEME_COLLECTION_BRIDGE = 1L;
    private static final String COLLECTION_ID = "collectionId";
    private static final String ATTR_1 = "attr1";
    private static final String ATTR_2 = "attr2";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile;
    @Mock private ImageFetcher mImageFetcher;
    @Mock private NtpThemeCollectionBridge.Natives mNatives;
    @Mock private Callback<Bitmap> mOnThemeImageSelectedCallback;

    @Captor private ArgumentCaptor<Callback<Bitmap>> mBitmapCallbackCaptor;

    private NtpCustomizationConfigManager mNtpCustomizationConfigManager;
    private NtpThemeCollectionManager mNtpThemeCollectionManager;
    private Context mContext;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        NtpCustomizationUtils.setImageFetcherForTesting(mImageFetcher);
        NtpThemeCollectionBridgeJni.setInstanceForTesting(mNatives);
        when(mNatives.init(any(), any())).thenReturn(NATIVE_NTP_THEME_COLLECTION_BRIDGE);
        mNtpCustomizationConfigManager = spy(new NtpCustomizationConfigManager());
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
        mNtpThemeCollectionManager.setThemeCollectionDailyRefreshed(COLLECTION_ID);
        GURL backgroundUrl = JUnitTestGURLs.URL_1;
        CustomBackgroundInfo info =
                new CustomBackgroundInfo(
                        backgroundUrl,
                        COLLECTION_ID,
                        /* isUploadedImage= */ false,
                        /* isDailyRefreshEnabled= */ true);
        Bitmap bitmap = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);

        mNtpThemeCollectionManager.onCustomBackgroundImageUpdated(info);

        verify(mImageFetcher).fetchImage(any(), mBitmapCallbackCaptor.capture());
        mBitmapCallbackCaptor.getValue().onResult(bitmap);

        // This is needed for the async task inside
        // saveBackgroundInfoForThemeCollectionOrUploadedImage
        RobolectricUtil.runAllBackgroundAndUi();

        verify(mOnThemeImageSelectedCallback).onResult(eq(bitmap));
        ArgumentCaptor<NtpBackgroundDataThemeCollection> captor =
                ArgumentCaptor.forClass(NtpBackgroundDataThemeCollection.class);
        verify(mNtpCustomizationConfigManager)
                .onBackgroundDataChanged(eq(mContext), captor.capture());
        assertEquals(bitmap, captor.getValue().getBitmap());
        assertEquals(info, captor.getValue().getCustomBackgroundInfo());
        assertNotNull(captor.getValue().getBackgroundImageInfo());
        // Verifying side effects of
        // NtpCustomizationUtils.saveBackgroundInfoForThemeCollectionOrUploadedImage
        assertTrue(NtpCustomizationUtils.createBackgroundImageFile().exists());
        assertEquals(
                info.collectionId,
                NtpCustomizationUtils.getCustomBackgroundInfoFromSharedPreference().collectionId);
        assertNotNull(NtpCustomizationUtils.readNtpBackgroundImageInfo());
        // Verifies primary color is picked and saved immediately.
        @ColorInt Integer primaryColor = captor.getValue().getPrimaryColor();
        assertNotNull(primaryColor);
        assertEquals(
                primaryColor,
                NtpCustomizationUtils.getCustomizedPrimaryColorFromSharedPreference());
    }

    @Test
    public void testConstructorWithCustomBackground() {
        CustomBackgroundInfo info =
                new CustomBackgroundInfo(
                        JUnitTestGURLs.URL_1,
                        "collection_id",
                        /* isUploadedImage= */ false,
                        /* isDailyRefreshEnabled= */ true);
        when(mNtpCustomizationConfigManager.getBackgroundType())
                .thenReturn(NtpCustomizationUtils.NtpBackgroundType.THEME_COLLECTION);
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
    public void testSetThemeCollectionImage() {
        createAndSetThemeCollectionImage();
    }

    @Test
    public void testCancelPendingSelection() {
        CollectionImage image = createAndSetThemeCollectionImage();

        CustomBackgroundInfo info =
                new CustomBackgroundInfo(
                        image.imageUrl,
                        image.collectionId,
                        /* isUploadedImage= */ false,
                        /* isDailyRefreshEnabled= */ false);
        mNtpThemeCollectionManager.onCustomBackgroundImageUpdated(info);
        verify(mImageFetcher).fetchImage(any(), mBitmapCallbackCaptor.capture());

        mNtpThemeCollectionManager.cancelPendingSelection();

        Bitmap bitmap = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);
        mBitmapCallbackCaptor.getValue().onResult(bitmap);
        RobolectricUtil.runAllBackgroundAndUi();

        verify(mOnThemeImageSelectedCallback, never()).onResult(any());
        verify(mNtpCustomizationConfigManager, never()).onBackgroundDataChanged(any(), any());
    }

    @Test
    public void testSetThemeCollectionDailyRefreshed() {
        mNtpThemeCollectionManager =
                new NtpThemeCollectionManager(mContext, mProfile, mOnThemeImageSelectedCallback);

        // 1. User enables daily refresh. This sets the runnable.
        mNtpThemeCollectionManager.setThemeCollectionDailyRefreshed(COLLECTION_ID);
        verify(mNatives)
                .setThemeCollectionDailyRefreshed(
                        eq(NATIVE_NTP_THEME_COLLECTION_BRIDGE), eq(COLLECTION_ID));

        // 2. The first image for the collection arrives.
        GURL backgroundUrl = JUnitTestGURLs.URL_1;
        CustomBackgroundInfo info =
                new CustomBackgroundInfo(
                        backgroundUrl,
                        COLLECTION_ID,
                        /* isUploadedImage= */ false,
                        /* isDailyRefreshEnabled= */ true);
        Bitmap bitmap = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);

        mNtpThemeCollectionManager.onCustomBackgroundImageUpdated(info);

        verify(mImageFetcher).fetchImage(any(), mBitmapCallbackCaptor.capture());
        mBitmapCallbackCaptor.getValue().onResult(bitmap);
        RobolectricUtil.runAllBackgroundAndUi();

        // 3. Verify the theme was set for today.
        verify(mOnThemeImageSelectedCallback).onResult(eq(bitmap));
        ArgumentCaptor<NtpBackgroundDataThemeCollection> captor =
                ArgumentCaptor.forClass(NtpBackgroundDataThemeCollection.class);
        verify(mNtpCustomizationConfigManager)
                .onBackgroundDataChanged(eq(mContext), captor.capture());
        assertEquals(bitmap, captor.getValue().getBitmap());
        assertEquals(info, captor.getValue().getCustomBackgroundInfo());
        assertNotNull(captor.getValue().getBackgroundImageInfo());
        assertTrue(NtpCustomizationUtils.createBackgroundImageFile().exists());

        // 4. Verify the runnable was executed to fetch the next image for tomorrow.
        verify(mNatives).fetchNextThemeCollectionImage(eq(NATIVE_NTP_THEME_COLLECTION_BRIDGE));
    }

    @Test
    public void testOnCustomBackgroundImageUpdated_destroyed() {
        mNtpThemeCollectionManager =
                new NtpThemeCollectionManager(mContext, mProfile, mOnThemeImageSelectedCallback);
        mNtpThemeCollectionManager.setThemeCollectionDailyRefreshed(COLLECTION_ID);
        GURL backgroundUrl = JUnitTestGURLs.URL_1;
        CustomBackgroundInfo info =
                new CustomBackgroundInfo(
                        backgroundUrl,
                        COLLECTION_ID,
                        /* isUploadedImage= */ false,
                        /* isDailyRefreshEnabled= */ true);
        Bitmap bitmap = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);

        mNtpThemeCollectionManager.onCustomBackgroundImageUpdated(info);

        verify(mImageFetcher).fetchImage(any(), mBitmapCallbackCaptor.capture());
        mNtpThemeCollectionManager.destroy();
        mBitmapCallbackCaptor.getValue().onResult(bitmap);
        verify(mOnThemeImageSelectedCallback, never()).onResult(any());
        verify(mNtpCustomizationConfigManager, never()).onBackgroundDataChanged(any(), any());
    }

    // --- Tests for tracking and canceling pending selections and in-flight downloads ---

    @Test
    public void testOnCustomBackgroundImageUpdated_whenNoThemeSelected_thenIgnoresUpdate() {
        mNtpThemeCollectionManager =
                new NtpThemeCollectionManager(mContext, mProfile, mOnThemeImageSelectedCallback);
        mNtpThemeCollectionManager.onCustomBackgroundImageUpdated(
                createBackgroundInfo(JUnitTestGURLs.URL_1, /* isDailyRefreshEnabled= */ false));

        verify(mImageFetcher, never()).fetchImage(any(), any());
    }

    @Test
    public void testCancelPendingSelection_beforeCustomBackgroundImageUpdated() {
        mNtpThemeCollectionManager =
                new NtpThemeCollectionManager(mContext, mProfile, mOnThemeImageSelectedCallback);
        mNtpThemeCollectionManager.setThemeCollectionDailyRefreshed(COLLECTION_ID);

        mNtpThemeCollectionManager.cancelPendingSelection();
        mNtpThemeCollectionManager.onCustomBackgroundImageUpdated(
                createBackgroundInfo(JUnitTestGURLs.URL_1, /* isDailyRefreshEnabled= */ true));

        verify(mImageFetcher, never()).fetchImage(any(), any());
    }

    @Test
    public void testDownload_supersededBySetThemeCollectionImage() {
        CollectionImage image = createAndSetThemeCollectionImage();
        mNtpThemeCollectionManager.onCustomBackgroundImageUpdated(
                createBackgroundInfo(JUnitTestGURLs.URL_1, /* isDailyRefreshEnabled= */ false));

        // A second selection starts while the first one is downloading.
        mNtpThemeCollectionManager.setThemeCollectionImage(image);
        CustomBackgroundInfo newerInfo =
                createBackgroundInfo(JUnitTestGURLs.URL_1, /* isDailyRefreshEnabled= */ false);
        mNtpThemeCollectionManager.onCustomBackgroundImageUpdated(newerInfo);

        verifyOnlySecondDownloadApplied(newerInfo);
    }

    @Test
    public void testDownload_supersededBySetThemeCollectionDailyRefreshed() {
        mNtpThemeCollectionManager =
                new NtpThemeCollectionManager(mContext, mProfile, mOnThemeImageSelectedCallback);
        mNtpThemeCollectionManager.setThemeCollectionDailyRefreshed(COLLECTION_ID);
        mNtpThemeCollectionManager.onCustomBackgroundImageUpdated(
                createBackgroundInfo(JUnitTestGURLs.URL_1, /* isDailyRefreshEnabled= */ true));

        // A second daily refresh selection starts while the first daily refresh image is still
        // downloading.
        mNtpThemeCollectionManager.setThemeCollectionDailyRefreshed(COLLECTION_ID);
        CustomBackgroundInfo newerInfo =
                createBackgroundInfo(JUnitTestGURLs.URL_2, /* isDailyRefreshEnabled= */ true);
        mNtpThemeCollectionManager.onCustomBackgroundImageUpdated(newerInfo);

        verifyOnlySecondDownloadApplied(newerInfo);
        verify(mNatives).fetchNextThemeCollectionImage(eq(NATIVE_NTP_THEME_COLLECTION_BRIDGE));
    }

    @Test
    public void testOnCustomBackgroundImageUpdated_consecutiveCallbacksCancelPreviousDownload() {
        mNtpThemeCollectionManager =
                new NtpThemeCollectionManager(mContext, mProfile, mOnThemeImageSelectedCallback);
        mNtpThemeCollectionManager.setThemeCollectionDailyRefreshed(COLLECTION_ID);
        mNtpThemeCollectionManager.setThemeCollectionDailyRefreshed(COLLECTION_ID);

        mNtpThemeCollectionManager.onCustomBackgroundImageUpdated(
                createBackgroundInfo(JUnitTestGURLs.URL_1, /* isDailyRefreshEnabled= */ true));
        CustomBackgroundInfo newerInfo =
                createBackgroundInfo(JUnitTestGURLs.URL_2, /* isDailyRefreshEnabled= */ true);
        mNtpThemeCollectionManager.onCustomBackgroundImageUpdated(newerInfo);

        verifyOnlySecondDownloadApplied(newerInfo);
        verify(mNatives).fetchNextThemeCollectionImage(eq(NATIVE_NTP_THEME_COLLECTION_BRIDGE));
    }

    /**
     * Completes two captured image downloads in order and verifies that the first (superseded)
     * result is dropped while the second one is applied with {@code expectedSecondInfo}.
     */
    private void verifyOnlySecondDownloadApplied(CustomBackgroundInfo expectedSecondInfo) {
        verify(mImageFetcher, times(2)).fetchImage(any(), mBitmapCallbackCaptor.capture());
        List<Callback<Bitmap>> callbacks = mBitmapCallbackCaptor.getAllValues();
        Callback<Bitmap> firstCallback = callbacks.get(0);
        Callback<Bitmap> secondCallback = callbacks.get(1);

        completeDownload(firstCallback);
        verify(mOnThemeImageSelectedCallback, never()).onResult(any());
        verify(mNtpCustomizationConfigManager, never()).onBackgroundDataChanged(any(), any());
        verify(mNatives, never()).fetchNextThemeCollectionImage(anyLong());

        completeDownload(secondCallback);
        ArgumentCaptor<NtpBackgroundDataThemeCollection> dataCaptor =
                ArgumentCaptor.forClass(NtpBackgroundDataThemeCollection.class);
        verify(mNtpCustomizationConfigManager)
                .onBackgroundDataChanged(eq(mContext), dataCaptor.capture());
        assertEquals(expectedSecondInfo, dataCaptor.getValue().getCustomBackgroundInfo());
        verify(mOnThemeImageSelectedCallback).onResult(any());
    }

    private static CustomBackgroundInfo createBackgroundInfo(
            GURL url, boolean isDailyRefreshEnabled) {
        return new CustomBackgroundInfo(
                url, COLLECTION_ID, /* isUploadedImage= */ false, isDailyRefreshEnabled);
    }

    private CollectionImage createAndSetThemeCollectionImage() {
        mNtpThemeCollectionManager =
                new NtpThemeCollectionManager(mContext, mProfile, mOnThemeImageSelectedCallback);
        CollectionImage image =
                new CollectionImage(
                        COLLECTION_ID,
                        JUnitTestGURLs.URL_1,
                        JUnitTestGURLs.URL_2,
                        List.of(ATTR_1, ATTR_2),
                        JUnitTestGURLs.URL_3);
        mNtpThemeCollectionManager.setThemeCollectionImage(image);
        verify(mNatives)
                .setThemeCollectionImage(
                        NATIVE_NTP_THEME_COLLECTION_BRIDGE,
                        COLLECTION_ID,
                        JUnitTestGURLs.URL_1,
                        JUnitTestGURLs.URL_2,
                        ATTR_1,
                        ATTR_2,
                        JUnitTestGURLs.URL_3);
        return image;
    }

    /** Completes the given image download and runs the tasks it posts. */
    private static void completeDownload(Callback<Bitmap> callback) {
        callback.onResult(Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888));
        RobolectricUtil.runAllBackgroundAndUi();
    }
}
