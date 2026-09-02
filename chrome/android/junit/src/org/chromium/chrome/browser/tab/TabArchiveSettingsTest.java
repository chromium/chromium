// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.DeviceInfo;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.tab.TabArchiveSettings.Observer;

/** Tests for {@link TabArchiveSettings}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabArchiveSettingsTest {
    private static final int AUTO_DELETE_TIME_DELTA_HOURS_DEFAULT = 90 * 24; // 60 days.

    private TabArchiveSettings mSettings;
    private SharedPreferencesManager mPrefsManager;

    @Before
    public void setUp() {
        mPrefsManager = ChromeSharedPreferences.getInstance();
        mSettings = new TabArchiveSettings(mPrefsManager);
        mSettings.resetSettingsForTesting();
    }

    @After
    public void tearDown() {
        DeviceInfo.resetIsDesktopForTesting();
    }

    @Test
    public void testDefaultSettings() {
        // Archive is disabled for tests, reset it to the default param value.
        mSettings.setArchiveEnabled(true);
        assertTrue(mSettings.getArchiveEnabled());
        assertEquals(
                TabArchiveSettings.DEFAULT_ARCHIVE_TIME_HOURS,
                mSettings.getArchiveTimeDeltaHours());
        // Auto-delete is disabled until the user has seen the promo or enables it manually.
        assertFalse(mSettings.isAutoDeleteEnabled());
        // Mock the user enabling auto-delete manually, and verify the settings are updated.
        mSettings.setAutoDeleteEnabled(true);
        assertTrue(mSettings.isAutoDeleteEnabled());
        assertEquals(AUTO_DELETE_TIME_DELTA_HOURS_DEFAULT, mSettings.getAutoDeleteTimeDeltaHours());
        assertEquals(
                TabArchiveSettings.DEFAULT_MAX_SIMULTANEOUS_ARCHIVES,
                mSettings.getMaxSimultaneousArchives());
    }

    @Test
    public void testAutoDeleteDisabledWhenArchiveDisabled() {
        mSettings.setArchiveEnabled(false);
        mSettings.setAutoDeleteEnabled(true);
        assertEquals(false, mSettings.isAutoDeleteEnabled());
    }

    @Test
    public void testNotifyObservers() throws Exception {
        CallbackHelper callbackHelper = new CallbackHelper();
        Observer obs =
                new Observer() {
                    @Override
                    public void onSettingChanged() {
                        callbackHelper.notifyCalled();
                    }
                };

        mSettings.addObserver(obs);
        mSettings.setArchiveTimeDeltaHours(1);
        RobolectricUtil.runAllBackgroundAndUi();
        callbackHelper.waitForNext();
    }

    @Test
    @EnableFeatures(
            ChromeFeatureList.ANDROID_TAB_DECLUTTER_ARCHIVE_ON_DESKTOP + ":force_disable/true")
    public void testForceDisableOnDesktop() {
        DeviceInfo.setIsDesktopForTesting(true);
        assertTrue(TabArchiveSettings.isArchiveForceDisabled());
        assertFalse(TabArchiveSettings.isArchiveDisabledByDefault());
        assertFalse(mSettings.getArchiveEnabled());

        // Even if explicitly set to true, it should remain disabled.
        mSettings.setArchiveEnabled(true);
        assertFalse(mSettings.getArchiveEnabled());
    }

    @Test
    @EnableFeatures(
            ChromeFeatureList.ANDROID_TAB_DECLUTTER_ARCHIVE_ON_DESKTOP + ":force_disable/true")
    public void testForceDisableOnNonDesktop() {
        DeviceInfo.setIsDesktopForTesting(false);
        assertFalse(TabArchiveSettings.isArchiveForceDisabled());
        assertFalse(TabArchiveSettings.isArchiveDisabledByDefault());

        mSettings.setArchiveEnabled(true);
        assertTrue(mSettings.getArchiveEnabled());
    }

    @Test
    @EnableFeatures(
            ChromeFeatureList.ANDROID_TAB_DECLUTTER_ARCHIVE_ON_DESKTOP + ":disable_by_default/true")
    public void testDisableByDefaultOnDesktop() {
        DeviceInfo.setIsDesktopForTesting(true);
        assertFalse(TabArchiveSettings.isArchiveForceDisabled());
        assertTrue(TabArchiveSettings.isArchiveDisabledByDefault());

        // Default should be false.
        assertFalse(mSettings.getArchiveEnabled());

        // User can enable it.
        mSettings.setArchiveEnabled(true);
        assertTrue(mSettings.getArchiveEnabled());

        // User can disable it.
        mSettings.setArchiveEnabled(false);
        assertFalse(mSettings.getArchiveEnabled());
    }

    @Test
    @EnableFeatures(
            ChromeFeatureList.ANDROID_TAB_DECLUTTER_ARCHIVE_ON_DESKTOP + ":disable_by_default/true")
    public void testDisableByDefaultOnNonDesktop() {
        DeviceInfo.setIsDesktopForTesting(false);
        assertFalse(TabArchiveSettings.isArchiveForceDisabled());
        assertFalse(TabArchiveSettings.isArchiveDisabledByDefault());

        mSettings.setArchiveEnabled(true);
        assertTrue(mSettings.getArchiveEnabled());
    }

    @Test
    public void testArchivedTabCount() throws Exception {
        assertEquals(0, mSettings.getArchivedTabCount());
        assertEquals(0, mSettings.getArchivedTabCountSupplier().get().intValue());

        CallbackHelper callbackHelper = new CallbackHelper();
        int[] observedCount = new int[1];
        Observer obs =
                new Observer() {
                    @Override
                    public void onSettingChanged() {}

                    @Override
                    public void onArchivedTabCountChanged(int count) {
                        observedCount[0] = count;
                        callbackHelper.notifyCalled();
                    }
                };

        mSettings.addObserver(obs);
        mSettings.setArchivedTabCount(5);
        RobolectricUtil.runAllBackgroundAndUi();
        assertEquals(5, mSettings.getArchivedTabCount());
        assertEquals(5, mSettings.getArchivedTabCountSupplier().get().intValue());
        assertEquals(5, observedCount[0]);
        assertEquals(1, callbackHelper.getCallCount());

        // Setting the same count should not trigger duplicate notifications.
        mSettings.setArchivedTabCount(5);
        RobolectricUtil.runAllBackgroundAndUi();
        assertEquals(1, callbackHelper.getCallCount());

        // Modifying the pref directly triggers notification via preference listener.
        mPrefsManager.writeInt(ChromePreferenceKeys.TAB_DECLUTTER_ARCHIVED_TAB_COUNT, 8);
        RobolectricUtil.runAllBackgroundAndUi();
        assertEquals(8, mSettings.getArchivedTabCount());
        assertEquals(8, mSettings.getArchivedTabCountSupplier().get().intValue());
        assertEquals(8, observedCount[0]);
        assertEquals(2, callbackHelper.getCallCount());

        mSettings.resetSettingsForTesting();
        assertEquals(0, mSettings.getArchivedTabCount());
        assertEquals(0, mSettings.getArchivedTabCountSupplier().get().intValue());
    }
}
