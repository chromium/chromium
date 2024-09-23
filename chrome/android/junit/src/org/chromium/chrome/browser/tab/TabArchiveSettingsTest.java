// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.FeatureList;
import org.chromium.base.FeatureList.TestValues;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.task.test.ShadowPostTask;
import org.chromium.base.task.test.ShadowPostTask.TestImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.tab.TabArchiveSettings.Observer;

/** Tests for {@link TabArchiveSettings}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = {ShadowPostTask.class})
public class TabArchiveSettingsTest {

    static final String ARCHIVE_ENABLED_PARAM = "android_tab_declutter_archive_enabled";
    static final String ARCHIVE_TIME_DELTA_PARAM = "android_tab_declutter_archive_time_delta_hours";
    static final int ARCHIVE_TIME_DELTA_HOURS_DEFAULT = 7 * 24;
    static final String AUTO_DELETE_ENABLED_PARAM = "android_tab_declutter_auto_delete_enabled";
    static final String AUTO_DELETE_TIME_DELTA_PARAM =
            "android_tab_declutter_auto_delete_time_delta_hours";
    static final int AUTO_DELETE_TIME_DELTA_HOURS_DEFAULT = 60 * 24;

    private TabArchiveSettings mSettings;
    private SharedPreferencesManager mPrefsManager;

    @Before
    public void setUp() {
        // Run posted tasks immediately.
        ShadowPostTask.setTestImpl(
                new TestImpl() {
                    @Override
                    public void postDelayedTask(
                            @TaskTraits int taskTraits, Runnable task, long delay) {
                        task.run();
                    }
                });

        mPrefsManager = ChromeSharedPreferences.getInstance();
        mSettings = new TabArchiveSettings(mPrefsManager);
        mSettings.resetSettingsForTesting();
    }

    @Test
    public void testSettings() {
        // Archive is disabled for tests, reset it to the default param value.
        mSettings.setArchiveEnabled(
                ChromeFeatureList.sAndroidTabDeclutterArchiveEnabled.getValue());
        assertEquals(
                ChromeFeatureList.sAndroidTabDeclutterArchiveEnabled.getValue(),
                mSettings.getArchiveEnabled());
        assertEquals(ARCHIVE_TIME_DELTA_HOURS_DEFAULT, mSettings.getArchiveTimeDeltaHours());
        assertEquals(false, mSettings.isAutoDeleteEnabled());
        assertEquals(AUTO_DELETE_TIME_DELTA_HOURS_DEFAULT, mSettings.getAutoDeleteTimeDeltaHours());

        mSettings.setArchiveEnabled(false);
        assertFalse(mSettings.getArchiveEnabled());

        mSettings.setArchiveTimeDeltaHours(1);
        assertEquals(1, mSettings.getArchiveTimeDeltaHours());

        mSettings.setArchiveEnabled(true);
        mSettings.setAutoDeleteEnabled(true);
        assertTrue(mSettings.isAutoDeleteEnabled());

        mSettings.setAutoDeleteTimeDeltaHours(1);
        assertEquals(1, mSettings.getArchiveTimeDeltaHours());
    }

    @Test
    public void testAutoDeleteDisabledWhenArchiveDisabled() {
        mSettings.setArchiveEnabled(false);
        mSettings.setAutoDeleteEnabled(true);
        assertEquals(false, mSettings.isAutoDeleteEnabled());
    }

    @Test
    public void testSettingsDefaultOverriddenByFinch() {
        // Archive is disabled for tests, reset it to the default param value.
        mSettings.setArchiveEnabled(
                ChromeFeatureList.sAndroidTabDeclutterArchiveEnabled.getValue());
        assertTrue(mSettings.getArchiveEnabled());
        assertFalse(mSettings.isAutoDeleteEnabled());

        TestValues testValues = new TestValues();
        testValues.addFieldTrialParamOverride(
                ChromeFeatureList.ANDROID_TAB_DECLUTTER, ARCHIVE_ENABLED_PARAM, "false");
        testValues.addFieldTrialParamOverride(
                ChromeFeatureList.ANDROID_TAB_DECLUTTER, ARCHIVE_TIME_DELTA_PARAM, "10");
        testValues.addFieldTrialParamOverride(
                ChromeFeatureList.ANDROID_TAB_DECLUTTER, AUTO_DELETE_ENABLED_PARAM, "true");
        testValues.addFieldTrialParamOverride(
                ChromeFeatureList.ANDROID_TAB_DECLUTTER, AUTO_DELETE_TIME_DELTA_PARAM, "20");
        FeatureList.setTestValues(testValues);

        // Archive is disabled for tests, reset it to the default param value.
        mSettings.setArchiveEnabled(
                ChromeFeatureList.sAndroidTabDeclutterArchiveEnabled.getValue());
        assertFalse(mSettings.getArchiveEnabled());
        assertFalse(mSettings.isAutoDeleteEnabled());
        mSettings.setArchiveEnabled(true);
        assertTrue(mSettings.isAutoDeleteEnabled());
        assertEquals(10, mSettings.getArchiveTimeDeltaHours());
        assertEquals(20, mSettings.getAutoDeleteTimeDeltaHours());

        mSettings.setArchiveTimeDeltaHours(1);
        assertEquals(1, mSettings.getArchiveTimeDeltaHours());

        mSettings.setAutoDeleteTimeDeltaHours(1);
        assertEquals(1, mSettings.getArchiveTimeDeltaHours());
    }

    @Test
    public void testNotifyObservers() throws Exception {
        CallbackHelper callbackHelper = new CallbackHelper();
        Observer obs =
                () -> {
                    callbackHelper.notifyCalled();
                };

        mSettings.addObserver(obs);
        mSettings.setArchiveTimeDeltaHours(1);
        callbackHelper.waitForNext();
    }
}
