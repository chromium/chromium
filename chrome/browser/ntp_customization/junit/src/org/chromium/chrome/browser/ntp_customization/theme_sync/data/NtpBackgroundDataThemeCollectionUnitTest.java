// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme_sync.data;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;

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
import org.chromium.chrome.browser.ntp_customization.theme.theme_collections.CustomBackgroundInfo;
import org.chromium.chrome.browser.ntp_customization.theme.upload_image.BackgroundImageInfo;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.NtpBackgroundDataBase.PlatformType;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

/** Tests for {@link NtpBackgroundDataThemeCollection}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class NtpBackgroundDataThemeCollectionUnitTest {
    @Test
    public void testEquals() {
        CustomBackgroundInfo info1 =
                new CustomBackgroundInfo(
                        GURL.emptyGURL(),
                        "id",
                        /* isUploadedImage= */ false,
                        /* isDailyRefreshEnabled= */ false);
        CustomBackgroundInfo info2 =
                new CustomBackgroundInfo(
                        GURL.emptyGURL(),
                        "id",
                        /* isUploadedImage= */ false,
                        /* isDailyRefreshEnabled= */ false);
        NtpBackgroundDataThemeCollection data1 =
                new NtpBackgroundDataThemeCollection(
                        PlatformType.ANDROID_LOCAL,
                        info1,
                        /* backgroundImageInfo= */ null,
                        /* bitmap= */ null,
                        Color.RED);
        NtpBackgroundDataThemeCollection data2 =
                new NtpBackgroundDataThemeCollection(
                        PlatformType.ANDROID_LOCAL,
                        info2,
                        /* backgroundImageInfo= */ null,
                        /* bitmap= */ null,
                        Color.RED);
        NtpBackgroundDataThemeCollection data3 =
                new NtpBackgroundDataThemeCollection(
                        PlatformType.ANDROID_LOCAL,
                        info1,
                        /* backgroundImageInfo= */ null,
                        /* bitmap= */ null,
                        Color.BLUE);

        assertEquals(data1, data2);
        assertNotEquals(data1, data3);
        assertEquals(data1.hashCode(), data2.hashCode());
    }

    @Test
    public void testToJsonAndFromJson() throws JSONException {
        @PlatformType int platformType = PlatformType.ANDROID_LOCAL;
        @NtpBackgroundType int backgroundType = NtpBackgroundType.THEME_COLLECTION;
        @ColorInt Integer primaryColor = Color.BLUE;
        GURL url = JUnitTestGURLs.URL_1;
        String collectionId = "collection";
        boolean isDailyRefreshEnabled = true;

        CustomBackgroundInfo info =
                new CustomBackgroundInfo(
                        url, collectionId, /* isUploadedImage= */ false, isDailyRefreshEnabled);
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
                        platformType, info, backgroundImageInfo, /* bitmap= */ null, primaryColor);

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

        assertNotNull(restored.getBackgroundImageInfo());
        assertEquals(
                portraitMatrix.toShortString(),
                restored.getBackgroundImageInfo().getPortraitMatrix().toShortString());
        assertEquals(
                landscapeMatrix.toShortString(),
                restored.getBackgroundImageInfo().getLandscapeMatrix().toShortString());
    }

    @Test
    public void testNullBackgroundImageInfo() throws JSONException {
        @PlatformType int platformType = PlatformType.ANDROID_LOCAL;
        @NtpBackgroundType int backgroundType = NtpBackgroundType.THEME_COLLECTION;
        @ColorInt Integer primaryColor = Color.BLUE;
        GURL url = JUnitTestGURLs.URL_1;
        String collectionId = "collection";
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
                        primaryColor);

        JSONObject json = data.toJson();
        NtpBackgroundDataThemeCollection restored = NtpBackgroundDataThemeCollection.fromJson(json);

        assertNull(restored.getBackgroundImageInfo());
    }
}
