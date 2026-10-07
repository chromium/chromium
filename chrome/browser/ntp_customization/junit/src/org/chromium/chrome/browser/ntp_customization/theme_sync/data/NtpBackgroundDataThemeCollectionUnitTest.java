// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme_sync.data;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

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
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationConfigManager;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundType;
import org.chromium.chrome.browser.ntp_customization.theme.theme_collections.CustomBackgroundInfo;
import org.chromium.chrome.browser.ntp_customization.theme.upload_image.BackgroundImageInfo;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.List;
import java.util.concurrent.atomic.AtomicReference;

/** Tests for {@link NtpBackgroundDataThemeCollection}. */
@RunWith(BaseRobolectricTestRunner.class)
public class NtpBackgroundDataThemeCollectionUnitTest {
    private static final String TEST_COLLECTION_ID = "id";
    private static final String TEST_OTHER_COLLECTION_ID = "other_id";
    private static final String TEST_ATTRIBUTION_LINE_1 = "attribution line 1";
    private static final String TEST_ATTRIBUTION_LINE_2 = "attribution line 2";
    private static final GURL TEST_ATTRIBUTION_ACTION_URL = JUnitTestGURLs.URL_3;
    private static final String TEST_FILE_ID_HASH = "file_id_hash";

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
        // Same image, but the attribution was localized by the device that selected it.
        CustomBackgroundInfo info4 =
                new CustomBackgroundInfo(
                        GURL.emptyGURL(),
                        TEST_COLLECTION_ID,
                        /* isUploadedImage= */ false,
                        /* isDailyRefreshEnabled= */ false,
                        TEST_ATTRIBUTION_LINE_1,
                        TEST_ATTRIBUTION_LINE_2,
                        TEST_ATTRIBUTION_ACTION_URL);
        NtpBackgroundDataThemeCollection data4 =
                new NtpBackgroundDataThemeCollection(
                        PlatformType.ANDROID,
                        info4,
                        /* backgroundImageInfo= */ null,
                        /* bitmap= */ null,
                        Color.RED,
                        /* fileIdHash= */ null);

        CustomBackgroundInfo infoOtherCollection =
                new CustomBackgroundInfo(
                        GURL.emptyGURL(),
                        TEST_OTHER_COLLECTION_ID,
                        /* isUploadedImage= */ false,
                        /* isDailyRefreshEnabled= */ false);
        NtpBackgroundDataThemeCollection dataOtherCollection =
                new NtpBackgroundDataThemeCollection(
                        PlatformType.ANDROID,
                        infoOtherCollection,
                        /* backgroundImageInfo= */ null,
                        /* bitmap= */ null,
                        Color.RED,
                        /* fileIdHash= */ null);

        NtpBackgroundDataThemeCollection dataNullColor =
                new NtpBackgroundDataThemeCollection(
                        PlatformType.ANDROID,
                        info1,
                        /* backgroundImageInfo= */ null,
                        /* bitmap= */ null,
                        /* primaryColor= */ null,
                        /* fileIdHash= */ null);

        NtpBackgroundDataThemeCollection dataFromDesktop =
                new NtpBackgroundDataThemeCollection(
                        PlatformType.DESKTOP,
                        info1,
                        /* backgroundImageInfo= */ null,
                        /* bitmap= */ null,
                        Color.RED,
                        /* fileIdHash= */ null);

        // equals() and hashCode() compare theme identity and ignore primaryColor.
        assertEquals(data1, data2);
        assertEquals(data1, data3);
        assertEquals(data1, dataNullColor);
        assertEquals(data1.hashCode(), data2.hashCode());
        assertEquals(data1.hashCode(), data3.hashCode());
        assertEquals(data1.hashCode(), dataNullColor.hashCode());
        assertNotEquals(data1, dataOtherCollection);

        // The attribution is localized per device and is not part of the theme identity.
        assertEquals(data1, data4);
        assertEquals(data1.hashCode(), data4.hashCode());

        // The device the image came from is not part of the theme identity.
        assertEquals(data1, dataFromDesktop);
        assertEquals(data1.hashCode(), dataFromDesktop.hashCode());
        assertTrue(data1.hasSameThemeAndColor(dataFromDesktop));

        // hasSameThemeAndColor() additionally requires primaryColor to match.
        assertTrue(data1.hasSameThemeAndColor(data2));
        assertTrue(data1.hasSameThemeAndColor(data4));
        assertFalse(data1.hasSameThemeAndColor(data3));
        assertFalse(data1.hasSameThemeAndColor(dataNullColor));
        assertFalse(data1.hasSameThemeAndColor(dataOtherCollection));

        // isBitmapSaved should not affect equality.
        data1.setIsBitmapSaved(/* isBitmapSaved= */ true);
        assertEquals(data1, data2);
        assertEquals(data1.hashCode(), data2.hashCode());
    }

    @Test
    public void testHasSameThemeAndCompatibleColor() {
        CustomBackgroundInfo info =
                new CustomBackgroundInfo(
                        GURL.emptyGURL(),
                        TEST_COLLECTION_ID,
                        /* isUploadedImage= */ false,
                        /* isDailyRefreshEnabled= */ false);
        CustomBackgroundInfo infoOtherCollection =
                new CustomBackgroundInfo(
                        GURL.emptyGURL(),
                        TEST_OTHER_COLLECTION_ID,
                        /* isUploadedImage= */ false,
                        /* isDailyRefreshEnabled= */ false);
        NtpBackgroundDataThemeCollection red = createThemeCollection(info, Color.RED);
        NtpBackgroundDataThemeCollection red2 = createThemeCollection(info, Color.RED);
        NtpBackgroundDataThemeCollection blue = createThemeCollection(info, Color.BLUE);
        NtpBackgroundDataThemeCollection noColor = createThemeCollection(info, null);
        NtpBackgroundDataThemeCollection noColor2 = createThemeCollection(info, null);
        NtpBackgroundDataThemeCollection otherCollectionNoColor =
                createThemeCollection(infoOtherCollection, null);

        // Two known colors must match.
        assertTrue(red.hasSameThemeAndCompatibleColor(red2));
        assertFalse(red.hasSameThemeAndCompatibleColor(blue));

        // A null color is "unknown" and is compatible with any color, in both directions.
        assertTrue(red.hasSameThemeAndCompatibleColor(noColor));
        assertTrue(noColor.hasSameThemeAndCompatibleColor(red));
        assertTrue(noColor.hasSameThemeAndCompatibleColor(noColor2));

        // The theme identity must still match.
        assertFalse(noColor.hasSameThemeAndCompatibleColor(otherCollectionNoColor));
        assertFalse(noColor.hasSameThemeAndCompatibleColor(null));
        assertFalse(
                noColor.hasSameThemeAndCompatibleColor(
                        new NtpBackgroundDataBase(PlatformType.ANDROID, "default")));

        // Unlike hasSameThemeAndCompatibleColor(), hasSameThemeAndColor() is strict about null.
        assertFalse(red.hasSameThemeAndColor(noColor));
    }

    @Test
    public void testToJsonAndFromJson() throws JSONException {
        testToJsonAndFromJsonImpl(
                /* fileIdHash= */ null,
                /* attributionLine1= */ null,
                /* attributionLine2= */ null,
                GURL.emptyGURL());
    }

    @Test
    public void testToJsonAndFromJson_withFileIdHash() throws JSONException {
        testToJsonAndFromJsonImpl(
                "test_hash",
                /* attributionLine1= */ null,
                /* attributionLine2= */ null,
                GURL.emptyGURL());
    }

    @Test
    public void testToJsonAndFromJson_withAttribution() throws JSONException {
        testToJsonAndFromJsonImpl(
                /* fileIdHash= */ null,
                TEST_ATTRIBUTION_LINE_1,
                TEST_ATTRIBUTION_LINE_2,
                TEST_ATTRIBUTION_ACTION_URL);
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
    public void testGetBitmapOrLoadImage_borrowsFromCurrentBackground() {
        NtpBackgroundDataThemeCollection currentData = setUpCurrentThemeCollection();

        // A recommended theme collection card only has a preview bitmap.
        Bitmap previewBitmap = Bitmap.createBitmap(5, 5, Bitmap.Config.ARGB_8888);
        NtpBackgroundDataThemeCollection card =
                new NtpBackgroundDataThemeCollection(
                        PlatformType.ANDROID, currentData.getCustomBackgroundInfo(), previewBitmap);

        // Verifies that the callback, which shows the thumbnail, gets the current theme's bitmap,
        // and that the card borrows everything needed to apply it.
        AtomicReference<Bitmap> callbackBitmap = new AtomicReference<>();
        card.getBitmapOrLoadImage(callbackBitmap::set);
        assertEquals(currentData.getBitmap(), callbackBitmap.get());
        assertEquals(currentData.getBitmap(), card.getBitmap());
        assertSame(currentData.getBackgroundImageInfo(), card.getBackgroundImageInfo());
        assertEquals(Color.RED, card.getPrimaryColor().intValue());
        assertEquals(TEST_FILE_ID_HASH, card.getFileIdHash());
        assertNotNull(card.getLastUploadImageFilePath());
    }

    @Test
    public void testGetBitmapOrLoadImage_borrowsFromCurrentBackground_keepsOwnData() {
        NtpBackgroundDataThemeCollection currentData = setUpCurrentThemeCollection();

        // A history entry loaded from prefs has no bitmap in memory yet.
        String ownFileIdHash = "own_file_id_hash";
        BackgroundImageInfo ownImageInfo =
                new BackgroundImageInfo(
                        new Matrix(),
                        new Matrix(),
                        /* portraitWindowSize= */ null,
                        /* landscapeWindowSize= */ null);
        NtpBackgroundDataThemeCollection historyEntry =
                new NtpBackgroundDataThemeCollection(
                        PlatformType.ANDROID,
                        currentData.getCustomBackgroundInfo(),
                        ownImageInfo,
                        /* bitmap= */ null,
                        Color.GREEN,
                        ownFileIdHash);

        // Verifies that only the bitmap is borrowed.
        historyEntry.getBitmapOrLoadImage(_ -> {});
        assertEquals(currentData.getBitmap(), historyEntry.getBitmap());
        assertSame(ownImageInfo, historyEntry.getBackgroundImageInfo());
        assertEquals(Color.GREEN, historyEntry.getPrimaryColor().intValue());
        assertEquals(ownFileIdHash, historyEntry.getFileIdHash());
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
            @Nullable String fileIdHash,
            @Nullable String attributionLine1,
            @Nullable String attributionLine2,
            GURL attributionActionUrl)
            throws JSONException {
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
                        attributionLine1,
                        attributionLine2,
                        attributionActionUrl);
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
        // An empty attribution action URL is not written, and a missing one is read back as empty.
        assertEquals(
                !attributionActionUrl.isEmpty(),
                json.getJSONObject(NtpBackgroundDataThemeCollection.CUSTOM_BACKGROUND_INFO_KEY)
                        .has(NtpBackgroundDataThemeCollection.ATTRIBUTION_ACTION_URL_KEY));
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
        assertEquals(attributionLine1, restored.getCustomBackgroundInfo().attributionLine1);
        assertEquals(attributionLine2, restored.getCustomBackgroundInfo().attributionLine2);
        assertEquals(attributionActionUrl, restored.getCustomBackgroundInfo().attributionActionUrl);
        assertEquals(data.getContentDescription(), restored.getContentDescription());

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
        testContentDescriptionImpl(
                TEST_ATTRIBUTION_LINE_1,
                TEST_ATTRIBUTION_LINE_2,
                TEST_ATTRIBUTION_LINE_1 + ", " + TEST_ATTRIBUTION_LINE_2);
    }

    @Test
    public void testContentDescription_withEmptySecondLine() {
        // An empty line is dropped rather than leaving a separator behind.
        testContentDescriptionImpl(
                TEST_ATTRIBUTION_LINE_1, /* attributionLine2= */ "", TEST_ATTRIBUTION_LINE_1);
    }

    @Test
    public void testContentDescription_withEmptyFirstLine() {
        testContentDescriptionImpl(
                /* attributionLine1= */ "", TEST_ATTRIBUTION_LINE_2, TEST_ATTRIBUTION_LINE_2);
    }

    @Test
    public void testContentDescription_withoutAttribution() {
        // The content description is still set, so that a recycled view does not keep a stale one.
        testContentDescriptionImpl(
                /* attributionLine1= */ null,
                /* attributionLine2= */ null,
                /* expectedContentDescription= */ "");
    }

    @Test
    public void testContentDescription_withEmptyAttribution() {
        testContentDescriptionImpl(
                /* attributionLine1= */ "",
                /* attributionLine2= */ "",
                /* expectedContentDescription= */ "");
    }

    private void testContentDescriptionImpl(
            @Nullable String attributionLine1,
            @Nullable String attributionLine2,
            String expectedContentDescription) {
        CustomBackgroundInfo info =
                new CustomBackgroundInfo(
                        JUnitTestGURLs.URL_1,
                        TEST_COLLECTION_ID,
                        /* isUploadedImage= */ false,
                        /* isDailyRefreshEnabled= */ false,
                        attributionLine1,
                        attributionLine2,
                        GURL.emptyGURL());
        NtpBackgroundDataThemeCollection data =
                new NtpBackgroundDataThemeCollection(
                        PlatformType.ANDROID,
                        info,
                        /* backgroundImageInfo= */ null,
                        /* bitmap= */ null,
                        Color.RED,
                        /* fileIdHash= */ null);

        assertEquals(expectedContentDescription, data.getContentDescription());

        String customDescription = "Custom Description";
        data.setContentDescription(customDescription);
        assertEquals(customDescription, data.getContentDescription());
    }

    @Test
    public void testCustomBackgroundInfo_createWithAttributions() {
        String attribution1 = "Attribution1";
        String attribution2 = "Attribution2";
        CustomBackgroundInfo info =
                CustomBackgroundInfo.createCustomBackgroundInfo(
                        JUnitTestGURLs.URL_1,
                        TEST_COLLECTION_ID,
                        /* isUploadedImage= */ false,
                        /* isDailyRefreshEnabled= */ false,
                        List.of(attribution1, attribution2),
                        TEST_ATTRIBUTION_ACTION_URL);
        assertEquals(attribution1, info.attributionLine1);
        assertEquals(attribution2, info.attributionLine2);
        assertEquals(TEST_ATTRIBUTION_ACTION_URL, info.attributionActionUrl);

        CustomBackgroundInfo infoNoAttributions =
                CustomBackgroundInfo.createCustomBackgroundInfo(
                        JUnitTestGURLs.URL_1,
                        TEST_COLLECTION_ID,
                        /* isUploadedImage= */ false,
                        /* isDailyRefreshEnabled= */ false,
                        List.of(),
                        GURL.emptyGURL());
        assertNull(infoNoAttributions.attributionLine1);
        assertNull(infoNoAttributions.attributionLine2);
    }

    @Test
    public void testCustomBackgroundInfo_equalsAndHashCode() {
        CustomBackgroundInfo info1 =
                new CustomBackgroundInfo(
                        JUnitTestGURLs.URL_1,
                        TEST_COLLECTION_ID,
                        /* isUploadedImage= */ false,
                        /* isDailyRefreshEnabled= */ false,
                        TEST_ATTRIBUTION_LINE_1,
                        TEST_ATTRIBUTION_LINE_2,
                        TEST_ATTRIBUTION_ACTION_URL);
        CustomBackgroundInfo info2 =
                new CustomBackgroundInfo(
                        JUnitTestGURLs.URL_1,
                        TEST_COLLECTION_ID,
                        /* isUploadedImage= */ false,
                        /* isDailyRefreshEnabled= */ false,
                        TEST_ATTRIBUTION_LINE_1,
                        TEST_ATTRIBUTION_LINE_2,
                        TEST_ATTRIBUTION_ACTION_URL);
        // The backdrop server localizes the attribution to the UI language of the device that
        // requested it, so the same image reaches this device with a different attribution, or
        // with no attribution or attribution link at all, depending on where it was selected.
        // Those still describe one and the same image.
        CustomBackgroundInfo infoNullAttribution =
                new CustomBackgroundInfo(
                        JUnitTestGURLs.URL_1,
                        TEST_COLLECTION_ID,
                        /* isUploadedImage= */ false,
                        /* isDailyRefreshEnabled= */ false,
                        /* attributionLine1= */ null,
                        /* attributionLine2= */ null,
                        GURL.emptyGURL());
        CustomBackgroundInfo infoDifferentAttribution =
                new CustomBackgroundInfo(
                        JUnitTestGURLs.URL_1,
                        TEST_COLLECTION_ID,
                        /* isUploadedImage= */ false,
                        /* isDailyRefreshEnabled= */ false,
                        "Different attribution line 1",
                        "Different attribution line 2",
                        TEST_ATTRIBUTION_ACTION_URL);
        CustomBackgroundInfo infoDifferentUrl =
                new CustomBackgroundInfo(
                        JUnitTestGURLs.URL_2,
                        TEST_COLLECTION_ID,
                        /* isUploadedImage= */ false,
                        /* isDailyRefreshEnabled= */ false,
                        TEST_ATTRIBUTION_LINE_1,
                        TEST_ATTRIBUTION_LINE_2,
                        TEST_ATTRIBUTION_ACTION_URL);
        CustomBackgroundInfo infoDifferentCollectionId =
                new CustomBackgroundInfo(
                        JUnitTestGURLs.URL_1,
                        TEST_OTHER_COLLECTION_ID,
                        /* isUploadedImage= */ false,
                        /* isDailyRefreshEnabled= */ false,
                        TEST_ATTRIBUTION_LINE_1,
                        TEST_ATTRIBUTION_LINE_2,
                        TEST_ATTRIBUTION_ACTION_URL);
        CustomBackgroundInfo infoDailyRefreshEnabled =
                new CustomBackgroundInfo(
                        JUnitTestGURLs.URL_1,
                        TEST_COLLECTION_ID,
                        /* isUploadedImage= */ false,
                        /* isDailyRefreshEnabled= */ true,
                        TEST_ATTRIBUTION_LINE_1,
                        TEST_ATTRIBUTION_LINE_2,
                        TEST_ATTRIBUTION_ACTION_URL);

        assertEquals(info1, info2);
        assertEquals(info1.hashCode(), info2.hashCode());

        assertEquals(info1, infoNullAttribution);
        assertEquals(info1.hashCode(), infoNullAttribution.hashCode());
        assertEquals(info1, infoDifferentAttribution);
        assertEquals(info1.hashCode(), infoDifferentAttribution.hashCode());

        assertNotEquals(info1, infoDifferentUrl);
        assertNotEquals(info1, infoDifferentCollectionId);
        assertNotEquals(info1, infoDailyRefreshEnabled);
    }

    private static NtpBackgroundDataThemeCollection createThemeCollection(
            CustomBackgroundInfo info, @Nullable @ColorInt Integer primaryColor) {
        return new NtpBackgroundDataThemeCollection(
                PlatformType.ANDROID,
                info,
                /* backgroundImageInfo= */ null,
                /* bitmap= */ null,
                primaryColor,
                /* fileIdHash= */ null);
    }

    /**
     * Sets a theme collection, which has a bitmap, a BackgroundImageInfo, a primary color and a
     * file ID hash, as the current background and returns it.
     */
    private static NtpBackgroundDataThemeCollection setUpCurrentThemeCollection() {
        Matrix matrix = new Matrix();
        matrix.setTranslate(10f, 0f);
        NtpBackgroundDataThemeCollection currentData =
                new NtpBackgroundDataThemeCollection(
                        PlatformType.ANDROID,
                        new CustomBackgroundInfo(
                                JUnitTestGURLs.URL_1,
                                TEST_COLLECTION_ID,
                                /* isUploadedImage= */ false,
                                /* isDailyRefreshEnabled= */ false),
                        new BackgroundImageInfo(
                                matrix,
                                new Matrix(matrix),
                                /* portraitWindowSize= */ null,
                                /* landscapeWindowSize= */ null),
                        Bitmap.createBitmap(10, 10, Bitmap.Config.ARGB_8888),
                        Color.RED,
                        TEST_FILE_ID_HASH);

        NtpCustomizationConfigManager configManager = mock(NtpCustomizationConfigManager.class);
        when(configManager.getNtpBackgroundData()).thenReturn(currentData);
        NtpCustomizationConfigManager.setInstanceForTesting(configManager);
        return currentData;
    }
}
