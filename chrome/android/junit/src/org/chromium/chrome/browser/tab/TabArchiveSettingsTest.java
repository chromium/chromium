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

import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.task.test.ShadowPostTask;
import org.chromium.base.task.test.ShadowPostTask.TestImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.tab.TabArchiveSettings.Observer;

/** Tests for {@link TabArchiveSettings}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = {ShadowPostTask.class})
public class TabArchiveSettingsTest {
    private static final int AUTO_DELETE_TIME_DELTA_HOURS_DEFAULT = 90 * 24; // 60 days.

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
                () -> {
                    callbackHelper.notifyCalled();
                };

        mSettings.addObserver(obs);
        mSettings.setArchiveTimeDeltaHours(1);
        callbackHelper.waitForNext();
    }
}
