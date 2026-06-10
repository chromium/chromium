// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme_sync.data;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;

import android.graphics.Color;
import android.graphics.Matrix;

import androidx.annotation.ColorInt;

import org.json.JSONException;
import org.json.JSONObject;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundType;
import org.chromium.chrome.browser.ntp_customization.theme.upload_image.BackgroundImageInfo;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.NtpBackgroundDataBase.PlatformType;

/** Tests for {@link NtpBackgroundDataUploadImage}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class NtpBackgroundDataUploadImageUnitTest {
    @Test
    public void testEquals() {
        BackgroundImageInfo info1 = new BackgroundImageInfo(new Matrix(), new Matrix(), null, null);
        BackgroundImageInfo info2 = new BackgroundImageInfo(new Matrix(), new Matrix(), null, null);
        NtpBackgroundDataUploadImage data1 =
                new NtpBackgroundDataUploadImage(
                        PlatformType.ANDROID_LOCAL, "path", info1, /* bitmap= */ null, Color.RED);
        NtpBackgroundDataUploadImage data2 =
                new NtpBackgroundDataUploadImage(
                        PlatformType.ANDROID_LOCAL, "path", info2, /* bitmap= */ null, Color.RED);
        NtpBackgroundDataUploadImage data3 =
                new NtpBackgroundDataUploadImage(
                        PlatformType.ANDROID_LOCAL, "path2", info1, /* bitmap= */ null, Color.RED);

        assertEquals(data1, data2);
        assertNotEquals(data1, data3);
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
        String filePath = "/another/path.png";
        @PlatformType int platformType = PlatformType.ANDROID_LOCAL;
        @ColorInt Integer primaryColor = Color.BLUE;

        NtpBackgroundDataUploadImage data =
                new NtpBackgroundDataUploadImage(
                        platformType,
                        filePath,
                        backgroundImageInfo,
                        /* bitmap= */ null,
                        primaryColor);

        JSONObject json = data.toJson();
        NtpBackgroundDataUploadImage restored = NtpBackgroundDataUploadImage.fromJson(json);

        assertEquals(platformType, restored.getPlatformType());
        assertEquals(NtpBackgroundType.IMAGE_FROM_DISK, restored.getBackgroundType());
        assertEquals(filePath, restored.getLastUploadImageFilePath());
        assertEquals(primaryColor, restored.getPrimaryColor());
        assertNotNull(restored.getBackgroundImageInfo());
        assertEquals(
                portraitMatrix.toShortString(),
                restored.getBackgroundImageInfo().getPortraitMatrix().toShortString());
        assertEquals(
                landscapeMatrix.toShortString(),
                restored.getBackgroundImageInfo().getLandscapeMatrix().toShortString());
    }
}
