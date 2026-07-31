// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme_sync.data;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.Matrix;

import androidx.annotation.ColorInt;

import org.json.JSONException;
import org.json.JSONObject;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationConfigManager;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundType;
import org.chromium.chrome.browser.ntp_customization.theme.upload_image.BackgroundImageInfo;

/** Tests for {@link NtpBackgroundDataUploadImage}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class NtpBackgroundDataUploadImageUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private NtpCustomizationConfigManager mNtpCustomizationConfigManager;
    @Mock private Callback<Bitmap> mCallback;
    @Mock private Bitmap mBitmap;

    private static final String FILE_ID_HASH = "fileIdHash";
    private static final String FILE_ID_HASH_1 = "fileIdHash1";
    private static final String FILE_ID_HASH_2 = "fileIdHash2";
    private static final String TEST_FILE_ID_HASH = "test_file_id_hash";

    @Before
    public void setUp() {
        NtpCustomizationConfigManager.setInstanceForTesting(mNtpCustomizationConfigManager);
    }

    @Test
    public void testEquals() {
        BackgroundImageInfo info1 = new BackgroundImageInfo(new Matrix(), new Matrix(), null, null);
        BackgroundImageInfo info2 = new BackgroundImageInfo(new Matrix(), new Matrix(), null, null);
        NtpBackgroundDataUploadImage data1 =
                new NtpBackgroundDataUploadImage(
                        PlatformType.ANDROID, info1, /* bitmap= */ null, Color.RED, FILE_ID_HASH_1);
        NtpBackgroundDataUploadImage data2 =
                new NtpBackgroundDataUploadImage(
                        PlatformType.ANDROID, info2, /* bitmap= */ null, Color.RED, FILE_ID_HASH_1);
        NtpBackgroundDataUploadImage data3 =
                new NtpBackgroundDataUploadImage(
                        PlatformType.ANDROID,
                        info1,
                        /* bitmap= */ null,
                        Color.BLUE,
                        FILE_ID_HASH_1);
        NtpBackgroundDataUploadImage data4 =
                new NtpBackgroundDataUploadImage(
                        PlatformType.ANDROID, info1, /* bitmap= */ null, Color.RED, FILE_ID_HASH_2);

        assertEquals(data1, data2);
        assertNotEquals(data1, data3);
        assertNotEquals(data1, data4);
        assertEquals(data1.hashCode(), data2.hashCode());

        // isBitmapSaved should not affect equality.
        data1.setIsBitmapSaved(/* isBitmapSaved= */ true);
        assertEquals(data1, data2);
        assertEquals(data1.hashCode(), data2.hashCode());
    }

    @Test
    public void testToJsonAndFromJson() throws JSONException {
        Matrix portraitMatrix = new Matrix();
        portraitMatrix.setTranslate(1f, 2f);
        Matrix landscapeMatrix = new Matrix();
        landscapeMatrix.setScale(0.5f, 0.5f);
        BackgroundImageInfo backgroundImageInfo =
                new BackgroundImageInfo(portraitMatrix, landscapeMatrix, null, null);
        String filePath =
                NtpCustomizationUtils.createUploadImageFileInDirForTesting(TEST_FILE_ID_HASH)
                        .getAbsolutePath();
        @PlatformType int platformType = PlatformType.ANDROID;
        @ColorInt Integer primaryColor = Color.BLUE;

        NtpBackgroundDataUploadImage data =
                new NtpBackgroundDataUploadImage(
                        platformType,
                        backgroundImageInfo,
                        /* bitmap= */ null,
                        primaryColor,
                        TEST_FILE_ID_HASH);

        JSONObject json = data.toJson();
        NtpBackgroundDataUploadImage restored = NtpBackgroundDataUploadImage.fromJson(json);

        assertEquals(platformType, restored.getPlatformType());
        assertEquals(NtpBackgroundType.IMAGE_FROM_DISK, restored.getBackgroundType());
        assertEquals(filePath, restored.getLastUploadImageFilePath());
        assertEquals(primaryColor, restored.getPrimaryColor());
        assertEquals(TEST_FILE_ID_HASH, restored.getFileIdHash());
        assertEquals(data.isBitmapSaved(), restored.isBitmapSaved());
        assertNotNull(restored.getBackgroundImageInfo());
        assertEquals(
                portraitMatrix.toShortString(),
                restored.getBackgroundImageInfo().getPortraitMatrix().toShortString());
        assertEquals(
                landscapeMatrix.toShortString(),
                restored.getBackgroundImageInfo().getLandscapeMatrix().toShortString());
    }

    @Test
    public void testGetImageBitmap() {
        BackgroundImageInfo info = new BackgroundImageInfo(new Matrix(), new Matrix(), null, null);
        NtpBackgroundDataUploadImage data =
                new NtpBackgroundDataUploadImage(
                        PlatformType.ANDROID,
                        info,
                        mBitmap,
                        /* primaryColor= */ null,
                        FILE_ID_HASH);
        assertEquals(mBitmap, data.getImageBitmapForTesting());

        NtpBackgroundDataUploadImage dataWithoutBitmap =
                new NtpBackgroundDataUploadImage(
                        PlatformType.ANDROID,
                        info,
                        /* bitmap= */ null,
                        /* primaryColor= */ null,
                        FILE_ID_HASH);
        assertNull(dataWithoutBitmap.getImageBitmapForTesting());
    }

    @Test
    public void testLoadImage_withBitmap() {
        BackgroundImageInfo info = new BackgroundImageInfo(new Matrix(), new Matrix(), null, null);
        NtpBackgroundDataUploadImage data =
                new NtpBackgroundDataUploadImage(
                        PlatformType.ANDROID,
                        info,
                        mBitmap,
                        /* primaryColor= */ null,
                        FILE_ID_HASH);

        data.getBitmapOrLoadImage(mCallback);
        verify(mCallback).onResult(mBitmap);
    }

    @Test
    public void testLoadImage_fromCurrentBackgroundData() {
        BackgroundImageInfo info = new BackgroundImageInfo(new Matrix(), new Matrix(), null, null);
        // The currentData has a bitmap.
        NtpBackgroundDataUploadImage currentData =
                new NtpBackgroundDataUploadImage(
                        PlatformType.ANDROID,
                        info,
                        mBitmap,
                        /* primaryColor= */ null,
                        FILE_ID_HASH);
        when(mNtpCustomizationConfigManager.getNtpBackgroundData()).thenReturn(currentData);

        // The testData does not have a bitmap, but is equal to currentData (same path).
        NtpBackgroundDataUploadImage testData =
                new NtpBackgroundDataUploadImage(
                        PlatformType.ANDROID,
                        info,
                        /* bitmap= */ null,
                        /* primaryColor= */ null,
                        FILE_ID_HASH);

        testData.getBitmapOrLoadImage(mCallback);
        verify(mCallback).onResult(mBitmap);
        // Also verify that testData now has the bitmap cached.
        assertEquals(mBitmap, testData.getImageBitmapForTesting());
    }
}
