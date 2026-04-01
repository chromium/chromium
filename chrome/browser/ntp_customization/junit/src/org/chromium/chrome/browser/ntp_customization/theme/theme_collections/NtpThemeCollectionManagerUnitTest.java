// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme.theme_collections;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
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
import org.chromium.base.test.RobolectricUtil;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationConfigManager;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils;
import org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpThemeColorInfo;
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
    public static final long NATIVE_NTP_THEME_COLLECTION_BRIDGE = 1L;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile;
    @Mock private ImageFetcher mImageFetcher;
    @Mock private NtpThemeCollectionBridge.Natives mNatives;
    @Mock private Callback<Bitmap> mOnThemeImageSelectedCallback;
    @Mock private NtpCustomizationConfigManager mNtpCustomizationConfigManager;

    @Captor private ArgumentCaptor<Callback<Bitmap>> mBitmapCallbackCaptor;

    private NtpThemeCollectionManager mNtpThemeCollectionManager;
    private Context mContext;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        NtpCustomizationUtils.setImageFetcherForTesting(mImageFetcher);
        NtpThemeCollectionBridgeJni.setInstanceForTesting(mNatives);
        when(mNatives.init(any(), any())).thenReturn(NATIVE_NTP_THEME_COLLECTION_BRIDGE);
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
                new CustomBackgroundInfo(
                        backgroundUrl,
                        "collectionId",
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
        verify(mNtpCustomizationConfigManager)
                .onThemeCollectionImageSelected(
                        eq(bitmap), eq(info), any(BackgroundImageInfo.class));
        // Verifying side effects of
        // NtpCustomizationUtils.saveBackgroundInfoForThemeCollectionOrUploadedImage
        assertTrue(NtpCustomizationUtils.createBackgroundImageFile().exists());
        assertEquals(
                info.collectionId,
                NtpCustomizationUtils.getCustomBackgroundInfoFromSharedPreference().collectionId);
        assertNotNull(NtpCustomizationUtils.readNtpBackgroundImageInfo());
        // Color picking is postponed.
        assertEquals(
                NtpThemeColorInfo.COLOR_NOT_SET,
                NtpCustomizationUtils.getCustomizedPrimaryColorFromSharedPreference());
    }

    @Test
    public void testOnCustomBackgroundImageUpdated_saveNextImageForDailyRefresh() {
        mNtpThemeCollectionManager =
                new NtpThemeCollectionManager(mContext, mProfile, mOnThemeImageSelectedCallback);
        // Set up current state: daily refresh is on for "collectionId".
        CustomBackgroundInfo currentInfo =
                new CustomBackgroundInfo(
                        JUnitTestGURLs.URL_1,
                        /* collectionId= */ "collectionId",
                        /* isUploadedImage= */ false,
                        /* isDailyRefreshEnabled= */ true);
        when(mNtpCustomizationConfigManager.getBackgroundType())
                .thenReturn(NtpCustomizationUtils.NtpBackgroundType.THEME_COLLECTION);
        when(mNtpCustomizationConfigManager.getCustomBackgroundInfo()).thenReturn(currentInfo);

        // A new image for the same collection arrives (simulating the pre-fetched image).
        CustomBackgroundInfo nextInfo =
                new CustomBackgroundInfo(
                        JUnitTestGURLs.URL_2,
                        /* collectionId= */ "collectionId",
                        /* isUploadedImage= */ false,
                        /* isDailyRefreshEnabled= */ true);
        Bitmap bitmap = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);

        mNtpThemeCollectionManager.onCustomBackgroundImageUpdated(nextInfo);

        verify(mImageFetcher).fetchImage(any(), mBitmapCallbackCaptor.capture());
        mBitmapCallbackCaptor.getValue().onResult(bitmap);
        RobolectricUtil.runAllBackgroundAndUi();

        // Verify it saves for daily refresh and does NOT apply it immediately.
        assertTrue(NtpCustomizationUtils.createDailyRefreshBackgroundImageFile().exists());
        assertNotNull(
                NtpCustomizationUtils.getDailyRefreshCustomBackgroundInfoFromSharedPreference());
        verify(mNtpCustomizationConfigManager, never())
                .onThemeCollectionImageSelected(any(), any(), any());
        verify(mOnThemeImageSelectedCallback, never()).onResult(any());
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
                        NATIVE_NTP_THEME_COLLECTION_BRIDGE,
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
        verify(mNatives).selectLocalBackgroundImage(NATIVE_NTP_THEME_COLLECTION_BRIDGE);
    }

    @Test
    public void testSetThemeCollectionDailyRefreshed() {
        mNtpThemeCollectionManager =
                new NtpThemeCollectionManager(mContext, mProfile, mOnThemeImageSelectedCallback);

        // 1. User enables daily refresh. This sets the runnable.
        String collectionId = "collectionId";
        mNtpThemeCollectionManager.setThemeCollectionDailyRefreshed(collectionId);
        verify(mNatives)
                .setThemeCollectionDailyRefreshed(
                        eq(NATIVE_NTP_THEME_COLLECTION_BRIDGE), eq(collectionId));

        // 2. The first image for the collection arrives.
        GURL backgroundUrl = JUnitTestGURLs.URL_1;
        CustomBackgroundInfo info =
                new CustomBackgroundInfo(
                        backgroundUrl,
                        collectionId,
                        /* isUploadedImage= */ false,
                        /* isDailyRefreshEnabled= */ true);
        Bitmap bitmap = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);

        // Mock config manager so isNextThemeCollectionImage returns false. This simulates the
        // first image for a collection arriving, not the prefetched "next day" image.
        when(mNtpCustomizationConfigManager.getBackgroundType())
                .thenReturn(NtpCustomizationUtils.NtpBackgroundType.DEFAULT);

        mNtpThemeCollectionManager.onCustomBackgroundImageUpdated(info);

        verify(mImageFetcher).fetchImage(any(), mBitmapCallbackCaptor.capture());
        mBitmapCallbackCaptor.getValue().onResult(bitmap);
        RobolectricUtil.runAllBackgroundAndUi();

        // 3. Verify the theme was set for today.
        verify(mOnThemeImageSelectedCallback).onResult(eq(bitmap));
        verify(mNtpCustomizationConfigManager)
                .onThemeCollectionImageSelected(
                        eq(bitmap), eq(info), any(BackgroundImageInfo.class));
        assertTrue(NtpCustomizationUtils.createBackgroundImageFile().exists());

        // 4. Verify the runnable was executed to fetch the next image for tomorrow.
        verify(mNatives).fetchNextThemeCollectionImage(eq(NATIVE_NTP_THEME_COLLECTION_BRIDGE));
    }

    @Test
    public void testOnCustomBackgroundImageUpdated_destroyed() {
        mNtpThemeCollectionManager =
                new NtpThemeCollectionManager(mContext, mProfile, mOnThemeImageSelectedCallback);
        GURL backgroundUrl = JUnitTestGURLs.URL_1;
        CustomBackgroundInfo info =
                new CustomBackgroundInfo(
                        backgroundUrl,
                        "collectionId",
                        /* isUploadedImage= */ false,
                        /* isDailyRefreshEnabled= */ true);
        Bitmap bitmap = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);

        mNtpThemeCollectionManager.onCustomBackgroundImageUpdated(info);

        verify(mImageFetcher).fetchImage(any(), mBitmapCallbackCaptor.capture());
        mNtpThemeCollectionManager.destroy();
        mBitmapCallbackCaptor.getValue().onResult(bitmap);
        verify(mOnThemeImageSelectedCallback, never()).onResult(any());
        verify(mNtpCustomizationConfigManager, never())
                .onThemeCollectionImageSelected(any(), any(), any());
    }

    // --- Tests for processing theme updates based on the user's current theme selection type ---

    // Case #1: No theme or daily refresh selected. Expect theme to be ignored.
    @Test
    public void testOnCustomBackgroundImageUpdated_whenNoThemeSelected_thenIgnoresUpdate() {
        mNtpThemeCollectionManager =
                new NtpThemeCollectionManager(mContext, mProfile, mOnThemeImageSelectedCallback);
        CustomBackgroundInfo info = createBackgroundInfo(/* isDailyRefresh= */ false);

        mNtpThemeCollectionManager.onCustomBackgroundImageUpdated(info);

        assertNull(mNtpThemeCollectionManager.getSelectingThemeCollectionImageForTesting());
        verifyThemeUpdateOutcome(info, /* shouldUpdateTheme= */ false);
    }

    // Case #2: Daily refresh update is applied when it is the active theme choice.
    @Test
    public void testOnCustomBackgroundImageUpdated_whenDailyRefreshEnabled_thenAppliesTheme() {
        selectDailyRefreshOptionForThemeCollection();
        CustomBackgroundInfo info = createBackgroundInfo(/* isDailyRefresh= */ true);

        mNtpThemeCollectionManager.onCustomBackgroundImageUpdated(info);

        verifyThemeUpdateOutcome(info, /* shouldUpdateTheme= */ true);
    }

    // Case #3a: Daily Refresh enabled, but user selected Chrome Default or Chrome Colors. Expect
    // theme to be ignored.
    @Test
    public void
            testOnCustomBackgroundImageUpdated_whenDailyRefreshAfterResetBackground_thenIgnoresTheme() {
        selectDailyRefreshOptionForThemeCollection();
        mNtpThemeCollectionManager.resetCustomBackground();
        CustomBackgroundInfo info = createBackgroundInfo(/* isDailyRefresh= */ true);

        mNtpThemeCollectionManager.onCustomBackgroundImageUpdated(info);

        verifyThemeUpdateOutcome(info, /* shouldUpdateTheme= */ false);
    }

    // Case #3b: Daily Refresh enabled, but user selected a local image. Expect theme to be ignored.
    @Test
    public void
            testOnCustomBackgroundImageUpdated_whenDailyRefreshAfterSelectLocalImage_thenIgnoresTheme() {
        selectDailyRefreshOptionForThemeCollection();
        mNtpThemeCollectionManager.selectLocalBackgroundImage();
        CustomBackgroundInfo info = createBackgroundInfo(/* isDailyRefresh= */ true);

        mNtpThemeCollectionManager.onCustomBackgroundImageUpdated(info);

        verifyThemeUpdateOutcome(info, /* shouldUpdateTheme= */ false);
    }

    // Case #4: Specific image selected, URL mismatches, no other selection. Expect theme to be
    // ignored.
    @Test
    public void testOnCustomBackgroundImageUpdated_whenUrlMismatchesSelection_thenIgnoresTheme() {
        mNtpThemeCollectionManager =
                new NtpThemeCollectionManager(mContext, mProfile, mOnThemeImageSelectedCallback);
        CollectionImage image =
                new CollectionImage(
                        "collectionId",
                        JUnitTestGURLs.URL_1,
                        JUnitTestGURLs.URL_2,
                        List.of("attr1"),
                        JUnitTestGURLs.URL_3);
        mNtpThemeCollectionManager.setThemeCollectionImage(image);
        CustomBackgroundInfo info =
                new CustomBackgroundInfo(JUnitTestGURLs.URL_2, "collectionId", false, false);

        mNtpThemeCollectionManager.onCustomBackgroundImageUpdated(info);

        verifyThemeUpdateOutcome(info, /* shouldUpdateTheme= */ false);
    }

    // Case #5a: Specific image selected, URL mismatches, and user selected Chrome Default. Expect
    // theme to be ignored.
    @Test
    public void
            testOnCustomBackgroundImageUpdated_whenUrlMismatchesAfterResetBackground_thenIgnoresTheme() {
        selectThemeCollectionImage();
        mNtpThemeCollectionManager.resetCustomBackground();
        CustomBackgroundInfo info =
                new CustomBackgroundInfo(JUnitTestGURLs.URL_2, "collectionId", false, false);

        mNtpThemeCollectionManager.onCustomBackgroundImageUpdated(info);

        verifyThemeUpdateOutcome(info, /* shouldUpdateTheme= */ false);
    }

    // Case #5b: Specific image selected, URL mismatches, and user selected a local image. Expect
    // theme to be ignored.
    @Test
    public void
            testOnCustomBackgroundImageUpdated_whenUrlMismatchesAfterSelectLocalImage_thenIgnoresTheme() {
        selectThemeCollectionImage(); // Selects image with URL_1
        mNtpThemeCollectionManager.selectLocalBackgroundImage();
        CustomBackgroundInfo info =
                new CustomBackgroundInfo(JUnitTestGURLs.URL_2, "collectionId", false, false);

        mNtpThemeCollectionManager.onCustomBackgroundImageUpdated(info);

        verifyThemeUpdateOutcome(info, /* shouldUpdateTheme= */ false);
    }

    // Case #6: Specific image selected, URL matches, no other selection. Expect theme to be
    // applied.
    @Test
    public void testOnCustomBackgroundImageUpdated_whenUrlMatchesSelection_thenAppliesTheme() {
        CollectionImage selectedImage = selectThemeCollectionImage();
        CustomBackgroundInfo info =
                new CustomBackgroundInfo(
                        selectedImage.imageUrl, selectedImage.collectionId, false, false);

        mNtpThemeCollectionManager.onCustomBackgroundImageUpdated(info);

        verifyThemeUpdateOutcome(info, /* shouldUpdateTheme= */ true);
    }

    // Case #7a: Specific image selected, URL matches, but user selected Chrome Default or Chrome
    // Colors. Expect theme to be ignored.
    @Test
    public void
            testOnCustomBackgroundImageUpdated_whenUrlMatchesAfterResetBackground_thenIgnoresTheme() {
        CollectionImage selectedImage = selectThemeCollectionImage();
        mNtpThemeCollectionManager.resetCustomBackground();
        CustomBackgroundInfo info =
                new CustomBackgroundInfo(
                        selectedImage.imageUrl, selectedImage.collectionId, false, false);

        mNtpThemeCollectionManager.onCustomBackgroundImageUpdated(info);

        verifyThemeUpdateOutcome(info, /* shouldUpdateTheme= */ false);
    }

    // Case #7b: Specific image selected, URL matches, but user selected a local image. Expect theme
    // to be ignored.
    @Test
    public void
            testOnCustomBackgroundImageUpdated_whenUrlMatchesAfterSelectLocalImage_thenIgnoresTheme() {
        CollectionImage selectedImage = selectThemeCollectionImage();
        mNtpThemeCollectionManager.selectLocalBackgroundImage();
        CustomBackgroundInfo info =
                new CustomBackgroundInfo(
                        selectedImage.imageUrl, selectedImage.collectionId, false, false);

        mNtpThemeCollectionManager.onCustomBackgroundImageUpdated(info);

        verifyThemeUpdateOutcome(info, /* shouldUpdateTheme= */ false);
    }

    private CustomBackgroundInfo createBackgroundInfo(boolean isDailyRefresh) {
        return new CustomBackgroundInfo(
                JUnitTestGURLs.URL_1,
                "collectionId",
                /* isUploadedImage= */ false,
                /* isDailyRefreshEnabled= */ isDailyRefresh);
    }

    private void selectDailyRefreshOptionForThemeCollection() {
        mNtpThemeCollectionManager =
                new NtpThemeCollectionManager(mContext, mProfile, mOnThemeImageSelectedCallback);

        mNtpThemeCollectionManager.setThemeCollectionDailyRefreshed("collectionId");

        assertNull(mNtpThemeCollectionManager.getSelectingThemeCollectionImageForTesting());
    }

    private CollectionImage selectThemeCollectionImage() {
        mNtpThemeCollectionManager =
                new NtpThemeCollectionManager(mContext, mProfile, mOnThemeImageSelectedCallback);
        CollectionImage image =
                new CollectionImage(
                        "collectionId",
                        JUnitTestGURLs.URL_1,
                        JUnitTestGURLs.URL_2,
                        List.of("attr1"),
                        JUnitTestGURLs.URL_3);
        mNtpThemeCollectionManager.setThemeCollectionImage(image);
        assertNotNull(mNtpThemeCollectionManager.getSelectingThemeCollectionImageForTesting());
        return image;
    }

    /**
     * Verifies the outcome of a theme update by checking if the theme was applied or ignored.
     *
     * @param info The {@link CustomBackgroundInfo} that was processed.
     * @param shouldUpdateTheme True if the theme should have been applied, false if ignored.
     */
    private void verifyThemeUpdateOutcome(CustomBackgroundInfo info, boolean shouldUpdateTheme) {
        Bitmap bitmap = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);
        verify(mImageFetcher).fetchImage(any(), mBitmapCallbackCaptor.capture());
        mBitmapCallbackCaptor.getValue().onResult(bitmap);
        RobolectricUtil.runAllBackgroundAndUi();

        assertEquals(shouldUpdateTheme, mNtpThemeCollectionManager.shouldProcessThemeUpdate(info));
        if (shouldUpdateTheme) {
            verify(mOnThemeImageSelectedCallback).onResult(eq(bitmap));
            verify(mNtpCustomizationConfigManager)
                    .onThemeCollectionImageSelected(
                            eq(bitmap), eq(info), any(BackgroundImageInfo.class));
        } else {
            verify(mOnThemeImageSelectedCallback, never()).onResult(any());
            verify(mNtpCustomizationConfigManager, never())
                    .onThemeCollectionImageSelected(any(), any(), any());
        }
    }
}
