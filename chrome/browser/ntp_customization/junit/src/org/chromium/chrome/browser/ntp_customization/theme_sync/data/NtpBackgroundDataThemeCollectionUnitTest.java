// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme_sync.data;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.Matrix;

import androidx.annotation.ColorInt;
import androidx.annotation.Nullable;

import org.json.JSONException;
import org.json.JSONObject;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundType;
import org.chromium.chrome.browser.ntp_customization.theme.theme_collections.CustomBackgroundInfo;
import org.chromium.chrome.browser.ntp_customization.theme.upload_image.BackgroundImageInfo;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.List;

/** Tests for {@link NtpBackgroundDataThemeCollection}. */
@RunWith(BaseRobolectricTestRunner.class)
public class NtpBackgroundDataThemeCollectionUnitTest {
    private static final String TEST_COLLECTION_ID = "id";
    private static final String TEST_ATTRIBUTION = "attribution";

    @Test
    public void testEquals() {
        CustomBackgroundInfo info1 =
                new CustomBackgroundInfo(
                        GURL.emptyGURL(),
                        TEST_COLLECTION_ID,
                        /* isUploadedImage= */ false,
                        /* isDailyRefreshEnabled= */ false);
        CustomBackgroundInfo info2 =
                new CustomBackgroundInfo(
                        GURL.emptyGURL(),
                        TEST_COLLECTION_ID,
                        /* isUploadedImage= */ false,
                        /* isDailyRefreshEnabled= */ false);
        NtpBackgroundDataThemeCollection data1 =
                new NtpBackgroundDataThemeCollection(
                        PlatformType.ANDROID,
                        info1,
                        /* backgroundImageInfo= */ null,
                        /* bitmap= */ null,
                        Color.RED,
                        /* fileIdHash= */ null);
        NtpBackgroundDataThemeCollection data2 =
                new NtpBackgroundDataThemeCollection(
                        PlatformType.ANDROID,
                        info2,
                        /* backgroundImageInfo= */ null,
                        /* bitmap= */ null,
                        Color.RED,
                        /* fileIdHash= */ null);
        NtpBackgroundDataThemeCollection data3 =
                new NtpBackgroundDataThemeCollection(
                        PlatformType.ANDROID,
                        info1,
                        /* backgroundImageInfo= */ null,
                        /* bitmap= */ null,
                        Color.BLUE,
                        /* fileIdHash= */ null);
        CustomBackgroundInfo info4 =
                new CustomBackgroundInfo(
                        GURL.emptyGURL(),
                        TEST_COLLECTION_ID,
                        /* isUploadedImage= */ false,
                        /* isDailyRefreshEnabled= */ false,
                        TEST_ATTRIBUTION);
        NtpBackgroundDataThemeCollection data4 =
                new NtpBackgroundDataThemeCollection(
                        PlatformType.ANDROID,
                        info4,
                        /* backgroundImageInfo= */ null,
                        /* bitmap= */ null,
                        Color.RED,
                        /* fileIdHash= */ null);

        assertEquals(data1, data2);
        assertNotEquals(data1, data3);
        assertEquals(data1, data4);
        assertEquals(data1.hashCode(), data2.hashCode());

        // isBitmapSaved should not affect equality.
        data1.setIsBitmapSaved(/* isBitmapSaved= */ true);
        assertEquals(data1, data2);
        assertEquals(data1.hashCode(), data2.hashCode());
    }

    @Test
    public void testToJsonAndFromJson() throws JSONException {
        testToJsonAndFromJsonImpl(/* fileIdHash= */ null, /* attribution= */ null);
    }

    @Test
    public void testToJsonAndFromJson_withFileIdHash() throws JSONException {
        testToJsonAndFromJsonImpl("test_hash", /* attribution= */ null);
    }

    @Test
    public void testToJsonAndFromJson_withAttribution() throws JSONException {
        testToJsonAndFromJsonImpl(/* fileIdHash= */ null, TEST_ATTRIBUTION);
    }

    @Test
    public void testSetPrimaryColor() {
        CustomBackgroundInfo info =
                new CustomBackgroundInfo(
                        GURL.emptyGURL(),
                        TEST_COLLECTION_ID,
                        /* isUploadedImage= */ false,
                        /* isDailyRefreshEnabled= */ false);
        NtpBackgroundDataThemeCollection data =
                new NtpBackgroundDataThemeCollection(
                        PlatformType.ANDROID,
                        info,
                        /* backgroundImageInfo= */ null,
                        /* bitmap= */ null,
                        Color.RED,
                        /* fileIdHash= */ null);
        assertEquals(Color.RED, data.getPrimaryColor().intValue());

        data.setPrimaryColor(Color.GREEN);
        assertEquals(Color.GREEN, data.getPrimaryColor().intValue());
    }

    @Test
    public void testGetBitmapOrLoadImage_withBitmap() {
        Bitmap bitmap = Bitmap.createBitmap(10, 10, Bitmap.Config.ARGB_8888);
        CustomBackgroundInfo info =
                new CustomBackgroundInfo(
                        GURL.emptyGURL(),
                        TEST_COLLECTION_ID,
                        /* isUploadedImage= */ false,
                        /* isDailyRefreshEnabled= */ false);
        NtpBackgroundDataThemeCollection data =
                new NtpBackgroundDataThemeCollection(
                        PlatformType.ANDROID,
                        info,
                        /* backgroundImageInfo= */ null,
                        bitmap,
                        Color.RED,
                        /* fileIdHash= */ null);

        data.getBitmapOrLoadImage((result) -> assertEquals(bitmap, result));
    }

    @Test
    public void testNullBackgroundImageInfo() throws JSONException {
        @PlatformType int platformType = PlatformType.ANDROID;
        @NtpBackgroundType int backgroundType = NtpBackgroundType.THEME_COLLECTION;
        @ColorInt Integer primaryColor = Color.BLUE;
        GURL url = JUnitTestGURLs.URL_1;
        String collectionId = TEST_COLLECTION_ID;
        boolean isDailyRefreshEnabled = true;

        CustomBackgroundInfo info =
                new CustomBackgroundInfo(
                        url, collectionId, /* isUploadedImage= */ false, isDailyRefreshEnabled);
        NtpBackgroundDataThemeCollection data =
                new NtpBackgroundDataThemeCollection(
                        platformType,
                        info,
                        /* backgroundImageInfo= */ null,
                        /* bitmap= */ null,
                        primaryColor,
                        /* fileIdHash= */ null);

        JSONObject json = data.toJson();
        NtpBackgroundDataThemeCollection restored = NtpBackgroundDataThemeCollection.fromJson(json);

        assertNull(restored.getBackgroundImageInfo());
    }

    private void testToJsonAndFromJsonImpl(
            @Nullable String fileIdHash, @Nullable String attribution) throws JSONException {
        @PlatformType int platformType = PlatformType.ANDROID;
        @NtpBackgroundType int backgroundType = NtpBackgroundType.THEME_COLLECTION;
        @ColorInt Integer primaryColor = Color.BLUE;
        GURL url = JUnitTestGURLs.URL_1;
        String collectionId = TEST_COLLECTION_ID;
        boolean isDailyRefreshEnabled = true;

        CustomBackgroundInfo info =
                new CustomBackgroundInfo(
                        url,
                        collectionId,
                        /* isUploadedImage= */ false,
                        isDailyRefreshEnabled,
                        attribution);
        Matrix portraitMatrix = new Matrix();
        portraitMatrix.setValues(new float[] {1, 0, 0, 0, 1, 0, 0, 0, 1});
        portraitMatrix.setTranslate(10f, 20f);
        Matrix landscapeMatrix = new Matrix();
        landscapeMatrix.setValues(new float[] {2, 0, 0, 0, 2, 0, 0, 0, 1});
        portraitMatrix.setTranslate(2f, 3f);

        BackgroundImageInfo backgroundImageInfo =
                new BackgroundImageInfo(portraitMatrix, landscapeMatrix, null, null);

        NtpBackgroundDataThemeCollection data =
                new NtpBackgroundDataThemeCollection(
                        platformType,
                        info,
                        backgroundImageInfo,
                        /* bitmap= */ null,
                        primaryColor,
                        fileIdHash);

        JSONObject json = data.toJson();
        NtpBackgroundDataThemeCollection restored = NtpBackgroundDataThemeCollection.fromJson(json);

        assertEquals(platformType, restored.getPlatformType());
        assertEquals(NtpBackgroundType.THEME_COLLECTION, restored.getBackgroundType());
        assertEquals(url, restored.getCustomBackgroundInfo().backgroundUrl);
        assertEquals(collectionId, restored.getCustomBackgroundInfo().collectionId);
        assertFalse(restored.getCustomBackgroundInfo().isUploadedImage);
        assertEquals(
                isDailyRefreshEnabled, restored.getCustomBackgroundInfo().isDailyRefreshEnabled);
        assertEquals(primaryColor, restored.getPrimaryColor());
        assertEquals(data.isBitmapSaved(), restored.isBitmapSaved());
        assertEquals(attribution, restored.getCustomBackgroundInfo().attribution);
        assertEquals(attribution, restored.getContentDescription());

        assertNotNull(restored.getBackgroundImageInfo());
        assertEquals(
                portraitMatrix.toShortString(),
                restored.getBackgroundImageInfo().getPortraitMatrix().toShortString());
        assertEquals(
                landscapeMatrix.toShortString(),
                restored.getBackgroundImageInfo().getLandscapeMatrix().toShortString());
        if (fileIdHash != null) {
            assertEquals(
                    NtpCustomizationUtils.createThemeCollectionImageFileInDirForTesting(fileIdHash)
                            .getAbsolutePath(),
                    restored.getLastUploadImageFilePath());
        } else {
            assertNull(restored.getLastUploadImageFilePath());
        }
    }

    @Test
    public void testIsBitmapSaved() throws JSONException {
        CustomBackgroundInfo info =
                new CustomBackgroundInfo(
                        GURL.emptyGURL(),
                        TEST_COLLECTION_ID,
                        /* isUploadedImage= */ false,
                        /* isDailyRefreshEnabled= */ false);
        NtpBackgroundDataThemeCollection data =
                new NtpBackgroundDataThemeCollection(
                        PlatformType.ANDROID,
                        info,
                        /* backgroundImageInfo= */ null,
                        /* bitmap= */ null,
                        Color.RED,
                        /* fileIdHash= */ null);

        // Should default to false.
        assertFalse(data.isBitmapSaved());

        data.setIsBitmapSaved(/* isBitmapSaved= */ true);
        assertTrue(data.isBitmapSaved());

        JSONObject json = data.toJson();
        NtpBackgroundDataThemeCollection restored = NtpBackgroundDataThemeCollection.fromJson(json);
        assertTrue(restored.isBitmapSaved());
    }

    @Test
    public void testContentDescription_withAttribution() {
        testContentDescriptionImpl(TEST_ATTRIBUTION);
    }

    @Test
    public void testContentDescription_withoutAttribution() {
        testContentDescriptionImpl(null);
    }

    private void testContentDescriptionImpl(@Nullable String attribution) {
        CustomBackgroundInfo info =
                new CustomBackgroundInfo(
                        JUnitTestGURLs.URL_1,
                        TEST_COLLECTION_ID,
                        /* isUploadedImage= */ false,
                        /* isDailyRefreshEnabled= */ false,
                        attribution);
        NtpBackgroundDataThemeCollection data =
                new NtpBackgroundDataThemeCollection(
                        PlatformType.ANDROID,
                        info,
                        /* backgroundImageInfo= */ null,
                        /* bitmap= */ null,
                        Color.RED,
                        /* fileIdHash= */ null);

        assertEquals(attribution, data.getContentDescription());

        String customDescription = "Custom Description";
        data.setContentDescription(customDescription);
        assertEquals(customDescription, data.getContentDescription());
    }

    @Test
    public void testCustomBackgroundInfo_createWithAttributions() {
        CustomBackgroundInfo info =
                CustomBackgroundInfo.createCustomBackgroundInfo(
                        JUnitTestGURLs.URL_1,
                        TEST_COLLECTION_ID,
                        /* isUploadedImage= */ false,
                        /* isDailyRefreshEnabled= */ false,
                        List.of("Attribution1", "Attribution2"));
        assertEquals("Attribution1, Attribution2", info.attribution);

        CustomBackgroundInfo infoNull =
                new CustomBackgroundInfo(
                        JUnitTestGURLs.URL_1,
                        TEST_COLLECTION_ID,
                        /* isUploadedImage= */ false,
                        /* isDailyRefreshEnabled= */ false,
                        /* attribution= */ null);
        assertNull(infoNull.attribution);
    }
}
