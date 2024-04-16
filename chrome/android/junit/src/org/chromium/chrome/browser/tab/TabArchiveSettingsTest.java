// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.FeatureList;
import org.chromium.base.FeatureList.TestValues;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;

/** Tests for {@link TabArchiveSettings}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabArchiveSettingsTest {

    static final String ARCHIVE_TIME_DELTA_PARAM = "android_tab_declutter_archive_time_delta_hours";
    static final int ARCHIVE_TIME_DELTA_HOURS_DEFAULT = 7 * 24;
    static final String AUTO_DELETE_TIME_DELTA_PARAM =
            "android_tab_declutter_auto_delete_time_delta_hours";
    static final int AUTO_DELETE_TIME_DELTA_HOURS_DEFAULT = 60 * 24;

    private TabArchiveSettings mSettings;

    @Before
    public void setUp() {
        mSettings = new TabArchiveSettings(ChromeSharedPreferences.getInstance());
        mSettings.resetSettingsForTesting();
    }

    @Test
    public void testSettings() {
        assertEquals(TabArchiveSettings.ARCHIVE_ENABLED_DEFAULT, mSettings.getArchiveEnabled());
        assertEquals(ARCHIVE_TIME_DELTA_HOURS_DEFAULT, mSettings.getArchiveTimeDeltaHours());
        assertEquals(
                TabArchiveSettings.AUTO_DELETE_ENABLED_DEFAULT, mSettings.isAutoDeleteEnabled());
        assertEquals(AUTO_DELETE_TIME_DELTA_HOURS_DEFAULT, mSettings.getAutoDeleteTimeDeltaHours());

        mSettings.setArchiveEnabled(false);
        assertFalse(mSettings.getArchiveEnabled());

        mSettings.setArchiveTimeDeltaHours(1);
        assertEquals(1, mSettings.getArchiveTimeDeltaHours());

        mSettings.setAutoDeleteEnabled(false);
        assertFalse(mSettings.isAutoDeleteEnabled());

        mSettings.setAutoDeleteTimeDeltaHours(1);
        assertEquals(1, mSettings.getArchiveTimeDeltaHours());
    }

    @Test
    public void testSettingsDefaultOverriddenByFinch() {
        TestValues testValues = new TestValues();
        testValues.addFieldTrialParamOverride(
                ChromeFeatureList.ANDROID_TAB_DECLUTTER, ARCHIVE_TIME_DELTA_PARAM, "10");
        testValues.addFieldTrialParamOverride(
                ChromeFeatureList.ANDROID_TAB_DECLUTTER, AUTO_DELETE_TIME_DELTA_PARAM, "20");
        FeatureList.setTestValues(testValues);

        assertEquals(10, mSettings.getArchiveTimeDeltaHours());
        assertEquals(20, mSettings.getAutoDeleteTimeDeltaHours());

        mSettings.setArchiveTimeDeltaHours(1);
        assertEquals(1, mSettings.getArchiveTimeDeltaHours());

        mSettings.setAutoDeleteTimeDeltaHours(1);
        assertEquals(1, mSettings.getArchiveTimeDeltaHours());
    }
}
