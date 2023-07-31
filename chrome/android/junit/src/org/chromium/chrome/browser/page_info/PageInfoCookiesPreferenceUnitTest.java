// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_info;

import static org.junit.Assert.assertEquals;

import android.text.format.DateUtils;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.page_info.PageInfoCookiesPreference;

/**
 * Tests the functionality of PageInfoCookiesPreference.java.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class PageInfoCookiesPreferenceUnitTest {
    @Test
    public void testDaysToExpirationCalculations() {
        // Same day before midnight.
        assertEquals(0,
                PageInfoCookiesPreference.calculateDaysUntilExpiration(
                        0, DateUtils.DAY_IN_MILLIS - 1));
        // Midnight of the next day.
        assertEquals(1,
                PageInfoCookiesPreference.calculateDaysUntilExpiration(0, DateUtils.DAY_IN_MILLIS));
        // A little after midnight on the 3rd day.
        assertEquals(3,
                PageInfoCookiesPreference.calculateDaysUntilExpiration(
                        0, 3 * DateUtils.DAY_IN_MILLIS + 1));
    }
}
