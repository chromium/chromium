// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Color;

import androidx.annotation.Px;
import androidx.core.content.ContextCompat;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RuntimeEnvironment;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.url.GURL;

/** Unit tests for {@link AutofillImageFetcherUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
public class AutofillImageFetcherUtilsTest {
    private Context mContext;

    @Before
    public void setUp() {
        mContext = RuntimeEnvironment.application;
    }

    @Test
    @SmallTest
    public void testGetPixAccountImageUrlWithParams() {
        GURL url = new GURL("https://www.example.com/icon");
        @Px
        int logoSize = AutofillImageFetcherUtils.getPixelSize(R.dimen.square_card_icon_side_length);
        String expectedFormattedUrl = "https://www.example.com/icon=w" + logoSize + "-h" + logoSize;

        GURL formattedUrl = AutofillImageFetcherUtils.getPixAccountImageUrlWithParams(url);

        assertEquals(expectedFormattedUrl, formattedUrl.getSpec());
    }

    @Test
    @SmallTest
    public void testTreatPixAccountImage_testAllEnhancementsApplied() {
        @Px
        int logoSize = AutofillImageFetcherUtils.getPixelSize(R.dimen.square_card_icon_side_length);
        @Px
        int iconCornerRadius =
                AutofillImageFetcherUtils.getPixelSize(R.dimen.large_card_icon_corner_radius);
        int borderColor = ContextCompat.getColor(mContext, R.color.baseline_neutral_90);
        Bitmap testImage = Bitmap.createBitmap(logoSize, logoSize, Bitmap.Config.ARGB_8888);
        // Expected treated image according to Pix specifications.
        Bitmap expectedTreatedTestImage =
                AutofillImageFetcherUtils.addBorder(
                        AutofillImageFetcherUtils.roundCorners(
                                AutofillImageFetcherUtils.addCenterAlignedBackground(
                                        AutofillImageFetcherUtils.roundCorners(
                                                testImage,
                                                AutofillImageFetcherUtils.getPixelSize(
                                                        R.dimen.square_card_icon_corner_radius)),
                                        AutofillImageFetcherUtils.getPixelSize(
                                                R.dimen.large_card_icon_width),
                                        AutofillImageFetcherUtils.getPixelSize(
                                                R.dimen.large_card_icon_height),
                                        Color.WHITE),
                                iconCornerRadius),
                        iconCornerRadius,
                        AutofillImageFetcherUtils.getPixelSize(R.dimen.card_icon_border_width),
                        borderColor);

        assertTrue(
                AutofillImageFetcherUtils.treatPixAccountImage(testImage)
                        .sameAs(expectedTreatedTestImage));
    }

    @Test
    @SmallTest
    public void testTreatPixAccountImage_testOutputImageDimensionsAreConstant() {
        @Px int iconWidth = AutofillImageFetcherUtils.getPixelSize(R.dimen.large_card_icon_width);
        @Px int iconHeight = AutofillImageFetcherUtils.getPixelSize(R.dimen.large_card_icon_height);
        Bitmap testImage = Bitmap.createBitmap(500, 400, Bitmap.Config.ARGB_8888);

        Bitmap treatedImage = AutofillImageFetcherUtils.treatPixAccountImage(testImage);

        assertEquals(iconWidth, treatedImage.getWidth());
        assertEquals(iconHeight, treatedImage.getHeight());
    }
}
