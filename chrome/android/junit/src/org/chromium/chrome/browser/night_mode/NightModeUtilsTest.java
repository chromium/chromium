// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.night_mode;

import static org.junit.Assert.assertTrue;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.HashSet;

/** Unit tests for {@link NightModeUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
public class NightModeUtilsTest {

    @Test
    public void testThemeSettingTitles() {
        final var context = ApplicationProvider.getApplicationContext();
        final var titles = new HashSet<>();

        // Ensure that a unique title is registered for every possible theme setting.
        for (int theme = 0; theme < ThemeType.NUM_ENTRIES; theme++) {
            final var title = NightModeUtils.getThemeSettingTitle(context, theme);
            assertTrue(titles.add(title));
        }
    }
}
