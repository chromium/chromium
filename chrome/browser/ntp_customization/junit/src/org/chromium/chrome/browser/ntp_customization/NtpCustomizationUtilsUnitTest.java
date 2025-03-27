// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization;

import static org.junit.Assert.assertEquals;

import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.getBackground;

import androidx.test.filters.SmallTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;

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
}
