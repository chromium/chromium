// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.getBackground;

import android.graphics.Bitmap;

import androidx.test.filters.SmallTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundImageType;

import java.io.File;

/** Unit tests for {@link NtpCustomizationUtils} */
@RunWith(BaseRobolectricTestRunner.class)
public class NtpCustomizationUtilsUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Test
    @SmallTest
    public void testGetBackgroundSizeOne() {
        int resId = getBackground(1, 0);
        assertEquals(R.drawable.ntp_customization_bottom_sheet_list_item_background_single, resId);
    }

    @Test
    @SmallTest
    public void testGetBackgroundSizeTwo() {
        int resId = getBackground(2, 0);
        assertEquals(R.drawable.ntp_customization_bottom_sheet_list_item_background_top, resId);

        resId = getBackground(2, 1);
        assertEquals(R.drawable.ntp_customization_bottom_sheet_list_item_background_bottom, resId);
    }

    @Test
    @SmallTest
    public void testGetBackgroundSizeThree() {
        int resId = getBackground(3, 0);
        assertEquals(R.drawable.ntp_customization_bottom_sheet_list_item_background_top, resId);

        resId = getBackground(3, 1);
        assertEquals(R.drawable.ntp_customization_bottom_sheet_list_item_background_middle, resId);

        resId = getBackground(3, 2);
        assertEquals(R.drawable.ntp_customization_bottom_sheet_list_item_background_bottom, resId);
    }

    @Test
    @SmallTest
    public void testGetBackgroundLargeSize() {
        int listSize = 10;
        int resId = getBackground(listSize, 0);
        assertEquals(R.drawable.ntp_customization_bottom_sheet_list_item_background_top, resId);

        for (int index = 1; index < listSize - 1; index++) {
            resId = getBackground(listSize, index);
            assertEquals(
                    R.drawable.ntp_customization_bottom_sheet_list_item_background_middle, resId);
        }

        resId = getBackground(listSize, listSize - 1);
        assertEquals(R.drawable.ntp_customization_bottom_sheet_list_item_background_bottom, resId);
    }

    @Test
    public void testGetAndSetNtpBackgroundImageType() {
        NtpCustomizationUtils.resetSharedPreferenceForTesting();
        assertEquals(
                NtpBackgroundImageType.DEFAULT, NtpCustomizationUtils.getNtpBackgroundImageType());

        @NtpBackgroundImageType
        int imageType = NtpCustomizationUtils.NtpBackgroundImageType.IMAGE_FROM_DISK;
        NtpCustomizationUtils.setNtpBackgroundImageType(imageType);

        assertEquals(imageType, NtpCustomizationUtils.getNtpBackgroundImageType());
    }

    @Test
    public void testDeleteBackgroundImageFile() {
        // Saves the bitmap to a file on the disk.
        Bitmap bitmap = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);
        NtpCustomizationUtils.saveBackgroundImageFile(bitmap);

        File file = NtpCustomizationUtils.getBackgroundImageFile();
        assertTrue(file.exists());

        NtpCustomizationUtils.deleteBackgroundImageFileImpl();
        assertFalse(file.exists());
    }

    @Test
    public void testSaveAndReadBackgroundImage() {
        // Saves the bitmap to a file on the disk.
        Bitmap bitmap = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);
        NtpCustomizationUtils.saveBackgroundImageFile(bitmap);

        // Reads the bitmap from the file.
        Bitmap bitmapResult = NtpCustomizationUtils.readNtpBackgroundImageImpl();

        // Verifies that the bitmap read from the file matches the original bitmap.
        assertTrue(bitmap.sameAs(bitmapResult));
    }
}
