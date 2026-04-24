// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme_history.data;

import static org.junit.Assert.assertEquals;

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
import org.chromium.chrome.browser.ntp_customization.theme_history.data.NtpBackgroundDataBase.PlatformType;

/** Tests for {@link NtpBackgroundDataUploadImage}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class NtpBackgroundDataUploadImageUnitTest {
    @Test
    public void testToJsonAndFromJson() throws JSONException {
        Matrix portraitMatrix = new Matrix();
        portraitMatrix.setTranslate(1f, 2f);
        Matrix landscapeMatrix = new Matrix();
        landscapeMatrix.setScale(0.5f, 0.5f);
        String filePath = "/another/path.png";
        @PlatformType int platformType = PlatformType.ANDROID_LOCAL;
        @ColorInt int primaryColor = Color.BLUE;

        NtpBackgroundDataUploadImage data =
                new NtpBackgroundDataUploadImage(
                        platformType, filePath, primaryColor, portraitMatrix, landscapeMatrix);

        JSONObject json = data.toJson();
        NtpBackgroundDataUploadImage restored = NtpBackgroundDataUploadImage.fromJson(json);

        assertEquals(platformType, restored.getPlatformType());
        assertEquals(NtpBackgroundType.IMAGE_FROM_DISK, restored.getBackgroundType());
        assertEquals(filePath, restored.getLastUploadImageFilePath());
        assertEquals(primaryColor, restored.getPrimaryColor());
        assertEquals(portraitMatrix, restored.getPortraitMatrix());
        assertEquals(landscapeMatrix, restored.getLandscapeMatrix());
    }
}
