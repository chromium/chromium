// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_info;

import static org.junit.Assert.assertEquals;

import android.text.format.DateUtils;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.page_info.PageInfoCookiesSettings;

import java.util.Calendar;
import java.util.Date;

/** Tests the functionality of PageInfoCookiesSettings.java. */
@RunWith(BaseRobolectricTestRunner.class)
public class PageInfoCookiesSettingsUnitTest {
    @Test
    public void testDaysToExpirationCalculations() {
        long currentTime = new Date(70, Calendar.JANUARY, 1).getTime();
        PageInfoCookiesSettings settings = new PageInfoCookiesSettings();
        // Same day before midnight.
        assertEquals(
                0,
                settings.daysUntilExpiration(
                        currentTime, currentTime + DateUtils.DAY_IN_MILLIS - 1));
        // Midnight of the next day.
        assertEquals(
                1,
                settings.daysUntilExpiration(currentTime, currentTime + DateUtils.DAY_IN_MILLIS));
        // A little after midnight on the 3rd day.
        assertEquals(
                3,
                settings.daysUntilExpiration(
                        currentTime, currentTime + 3 * DateUtils.DAY_IN_MILLIS + 1));
    }
}
