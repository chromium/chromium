// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;

import android.content.Context;
import android.view.ContextThemeWrapper;

import androidx.annotation.ColorInt;
import androidx.core.content.ContextCompat;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;

@RunWith(BaseRobolectricTestRunner.class)
public class TabUiThemeProviderUnitTest {
    private Context mContext;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ContextUtils.getApplicationContext(), R.style.Theme_BrowserUI_DayNight);
    }

    @Test
    public void testGetTabGridDialogBackgroundColor() {
        @ColorInt
        int gtsBackgroundColor =
                TabUiThemeProvider.getTabGridDialogBackgroundColor(
                        mContext, /* isIncognito= */ false);
        assertEquals(
                ContextCompat.getColor(mContext, R.color.tab_grid_dialog_bg_color),
                gtsBackgroundColor);

        @ColorInt
        int gtsBackgroundColorIncognito =
                TabUiThemeProvider.getTabGridDialogBackgroundColor(
                        mContext, /* isIncognito= */ true);
        assertEquals(
                ContextCompat.getColor(mContext, R.color.gm3_baseline_surface_container_dark),
                gtsBackgroundColorIncognito);
    }
}
