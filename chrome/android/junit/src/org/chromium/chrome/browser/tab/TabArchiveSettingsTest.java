// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;

import static org.chromium.chrome.browser.tab.TabArchiveSettings.ARCHIVE_ENABLED_DEFAULT;
import static org.chromium.chrome.browser.tab.TabArchiveSettings.ARCHIVE_TIME_DELTA_HOURS_DEFAULT;
import static org.chromium.chrome.browser.tab.TabArchiveSettings.AUTO_DELETE_ENABLED_DEFAULT;
import static org.chromium.chrome.browser.tab.TabArchiveSettings.AUTO_DELETE_TIME_DELTA_HOURS_DEFAULT;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;

/** Tests for {@link TabArchiveSettings}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabArchiveSettingsTest {

    private SharedPreferencesManager mSharedPrefs;

    @Before
    public void setUp() {
        mSharedPrefs = ChromeSharedPreferences.getInstance();
    }

    @Test
    public void testSettings() {
        TabArchiveSettings settings = new TabArchiveSettings(mSharedPrefs);
        assertEquals(ARCHIVE_ENABLED_DEFAULT, settings.getArchiveEnabled());
        assertEquals(ARCHIVE_TIME_DELTA_HOURS_DEFAULT, settings.getArchiveTimeDeltaHours());
        assertEquals(AUTO_DELETE_ENABLED_DEFAULT, settings.isAutoDeleteEnabled());
        assertEquals(AUTO_DELETE_TIME_DELTA_HOURS_DEFAULT, settings.getAutoDeleteTimeDeltaHours());

        settings.setArchiveEnabled(false);
        assertFalse(settings.getArchiveEnabled());

        settings.setArchiveTimeDeltaHours(1);
        assertEquals(1, settings.getArchiveTimeDeltaHours());

        settings.setAutoDeleteEnabled(false);
        assertFalse(settings.isAutoDeleteEnabled());

        settings.setAutoDeleteTimeDeltaHours(1);
        assertEquals(1, settings.getArchiveTimeDeltaHours());
    }
}
