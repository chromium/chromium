// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme_sync.data;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.ContentResolver;
import android.content.Context;
import android.database.Cursor;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.Matrix;
import android.graphics.Point;
import android.net.Uri;
import android.provider.OpenableColumns;
import android.view.ContextThemeWrapper;

import androidx.annotation.Nullable;
import androidx.test.core.app.ApplicationProvider;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundType;
import org.chromium.chrome.browser.ntp_customization.R;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.NtpBackgroundDataBase.PlatformType;

import java.io.File;

/** Tests for {@link NtpBackgroundDataUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class NtpBackgroundDataUtilsUnitTest {
    private Context mContext;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
    }

    @Test
    public void testFromJson_Subtypes() throws JSONException {
        // Test that fromJson polymorphic factory works.

        // NtpBackgroundDataColor case.
        JSONObject colorJson = new JSONObject();
        colorJson.put(NtpBackgroundDataBase.PLATFORM_TYPE_KEY, PlatformType.DESKTOP);
        colorJson.put(NtpBackgroundDataBase.BACKGROUND_TYPE_KEY, NtpBackgroundType.CHROME_COLOR);
        colorJson.put(NtpBackgroundDataColor.THEME_COLOR_ID_KEY, 1);
        colorJson.put(NtpBackgroundDataColor.IS_DAILY_REFRESH_ENABLED_KEY, true);

        NtpBackgroundDataBase colorData = NtpBackgroundDataUtils.fromJson(mContext, colorJson);
        assertTrue(colorData instanceof NtpBackgroundDataColor);
        assertEquals(1, ((NtpBackgroundDataColor) colorData).getThemeColorId());

        // NtpBackgroundDataCustomizedColor case.
        JSONObject customColorJson = new JSONObject();
        customColorJson.put(NtpBackgroundDataBase.PLATFORM_TYPE_KEY, PlatformType.ANDROID_LOCAL);
        customColorJson.put(
                NtpBackgroundDataBase.BACKGROUND_TYPE_KEY, NtpBackgroundType.COLOR_FROM_HEX);
        customColorJson.put(
                NtpBackgroundDataCustomizedColor.PRIMARY_COLOR_LIGHT_MODE_KEY, Color.GREEN);
        customColorJson.put(
                NtpBackgroundDataCustomizedColor.PRIMARY_COLOR_DARK_MODE_KEY, Color.BLUE);
        customColorJson.put(
                NtpBackgroundDataCustomizedColor.NTP_BACKGROUND_COLOR_LIGHT_MODE_KEY, Color.YELLOW);
        customColorJson.put(
                NtpBackgroundDataCustomizedColor.NTP_BACKGROUND_COLOR_DARK_MODE_KEY, Color.BLACK);

        NtpBackgroundDataBase customColorData =
                NtpBackgroundDataUtils.fromJson(mContext, customColorJson);
        assertTrue(customColorData instanceof NtpBackgroundDataCustomizedColor);
        assertEquals(
                Color.GREEN,
                ((NtpBackgroundDataCustomizedColor) customColorData).getPrimaryColorLight());
        assertEquals(
                Color.BLUE,
                ((NtpBackgroundDataCustomizedColor) customColorData).getPrimaryColorDark());
        assertEquals(
                Color.YELLOW,
                ((NtpBackgroundDataCustomizedColor) customColorData).getNtpBackgroundColorLight());
        assertEquals(
                Color.BLACK,
                ((NtpBackgroundDataCustomizedColor) customColorData).getNtpBackgroundColorDark());
    }

    @Test
    public void testMatrixConversion() throws JSONException {
        Matrix matrix = new Matrix();
        float[] values = {1, 2, 3, 4, 5, 6, 7, 8, 9};
        matrix.setValues(values);

        JSONArray jsonArray = NtpBackgroundDataUtils.matrixToJsonArray(matrix);
        assertNotNull(jsonArray);
        assertEquals(9, jsonArray.length());
        for (int i = 0; i < 9; i++) {
            assertEquals(values[i], (float) jsonArray.getDouble(i), 0.001f);
        }

        Matrix restoredMatrix = NtpBackgroundDataUtils.jsonArrayToMatrix(jsonArray);
        assertNotNull(restoredMatrix);
        float[] restoredValues = new float[9];
        restoredMatrix.getValues(restoredValues);
        for (int i = 0; i < 9; i++) {
            assertEquals(values[i], restoredValues[i], 0.001f);
        }

        assertNull(NtpBackgroundDataUtils.matrixToJsonArray(null));
        assertNull(NtpBackgroundDataUtils.jsonArrayToMatrix(null));
    }

    @Test
    public void testPointConversion() throws JSONException {
        Point point = new Point(123, 456);

        JSONArray jsonArray = NtpBackgroundDataUtils.pointToJsonArray(point);
        assertNotNull(jsonArray);
        assertEquals(2, jsonArray.length());
        assertEquals(123, jsonArray.getInt(0));
        assertEquals(456, jsonArray.getInt(1));

        Point restoredPoint = NtpBackgroundDataUtils.jsonArrayToPoint(jsonArray);
        assertNotNull(restoredPoint);
        assertEquals(point, restoredPoint);

        assertNull(NtpBackgroundDataUtils.pointToJsonArray(null));
        assertNull(NtpBackgroundDataUtils.jsonArrayToPoint(null));
    }

    @Test
    public void testJsonArrayToPoint_invalidLength_returnsNull() throws JSONException {
        JSONArray jsonArray = new JSONArray();
        jsonArray.put(1);
        assertNull(NtpBackgroundDataUtils.jsonArrayToPoint(jsonArray));
    }

    @Test
    public void testGetMetadataFingerprint_nullUri() {
        assertEquals("", NtpBackgroundDataUtils.getMetadataFingerprint(mContext, /* uri= */ null));
    }

    @Test
    public void testGetMetadataFingerprint_success() {
        Cursor cursor = mock(Cursor.class);
        when(cursor.moveToFirst()).thenReturn(true);
        when(cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME)).thenReturn(0);
        when(cursor.getColumnIndex(OpenableColumns.SIZE)).thenReturn(1);
        when(cursor.getString(0)).thenReturn("test_image.jpg");
        when(cursor.getLong(1)).thenReturn(12345L);

        runGetMetadataFingerprintTestImpl(
                cursor, /* expectQueryException= */ false, "test_image.jpg_12345");
    }

    @Test
    public void testGetMetadataFingerprint_queryThrowsException() {
        runGetMetadataFingerprintTestImpl(
                /* cursor= */ null, /* expectQueryException= */ true, "fallback_name.png_-1");
    }

    @Test
    public void testGetMetadataFingerprint_nullCursor() {
        runGetMetadataFingerprintTestImpl(
                /* cursor= */ null, /* expectQueryException= */ false, "fallback_name.png_-1");
    }

    private void runGetMetadataFingerprintTestImpl(
            @Nullable Cursor cursor, boolean expectQueryException, String expectedFingerprint) {
        ContentResolver contentResolver = mock(ContentResolver.class);
        Context context = spy(mContext);
        when(context.getContentResolver()).thenReturn(contentResolver);
        Uri uri = mock(Uri.class);
        when(uri.getLastPathSegment()).thenReturn("fallback_name.png");

        if (expectQueryException) {
            when(contentResolver.query(eq(uri), any(), any(), any(), any()))
                    .thenThrow(new SecurityException("Test exception"));
        } else {
            when(contentResolver.query(eq(uri), any(), any(), any(), any())).thenReturn(cursor);
        }

        String fingerprint = NtpBackgroundDataUtils.getMetadataFingerprint(context, uri);
        assertEquals(expectedFingerprint, fingerprint);

        if (cursor != null && !expectQueryException) {
            verify(cursor).close();
        }
    }

    @Test
    public void testLoadImage_defaultPath() {
        testLoadImageImpl(/* filePath= */ null);
    }

    @Test
    public void testLoadImage_customPath() {
        File customFile =
                NtpCustomizationUtils.createUploadImageFileInDirForTesting("customImage.png");
        testLoadImageImpl(customFile.getAbsolutePath());
    }

    private void testLoadImageImpl(@Nullable String filePath) {
        Bitmap bitmap = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);
        File targetFile;
        if (filePath == null || filePath.isEmpty()) {
            targetFile = NtpCustomizationUtils.createBackgroundImageFile();
        } else {
            targetFile = new File(filePath);
        }
        NtpCustomizationUtils.saveBitmapImageToFile(bitmap, targetFile);
        RobolectricUtil.runAllBackgroundAndUi();

        final Bitmap[] receivedBitmap = new Bitmap[1];
        Callback<Bitmap> callback =
                (result) -> {
                    receivedBitmap[0] = result;
                };
        NtpBackgroundDataUtils.loadImage(callback, filePath);
        RobolectricUtil.runAllBackgroundAndUi();

        assertNotNull(receivedBitmap[0]);
        assertTrue(bitmap.sameAs(receivedBitmap[0]));

        if (filePath == null || filePath.isEmpty()) {
            NtpCustomizationUtils.maybeDeleteFile(targetFile);
        } else {
            NtpCustomizationUtils.deleteThemeImageFileDir(
                    NtpCustomizationUtils.NTP_UPLOAD_IMAGES_DIR);
        }
    }
}
