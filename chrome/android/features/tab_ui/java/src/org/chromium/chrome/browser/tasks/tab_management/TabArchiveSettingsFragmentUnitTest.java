// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.app.Activity;

import androidx.fragment.app.FragmentManager;
import androidx.lifecycle.Lifecycle.State;
import androidx.test.core.app.ActivityScenario;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.tab.TabArchiveSettings;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.ui.base.TestActivity;

/** Unit tests for {@link TabsSettings}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabArchiveSettingsFragmentUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public JniMocker mJniMocker = new JniMocker();

    private ActivityScenario<TestActivity> mActivityScenario;
    private TestActivity mActivity;
    private TabArchiveSettings mArchiveSettings;

    @Before
    public void setUp() {
        mArchiveSettings = new TabArchiveSettings(ChromeSharedPreferences.getInstance());

        mActivityScenario = ActivityScenario.launch(TestActivity.class);
        mActivityScenario.onActivity(this::onActivity);
    }

    @After
    public void tearDown() {
        mActivityScenario.close();
    }

    private void onActivity(Activity activity) {
        mActivity = (TestActivity) activity;
    }

    private TabArchiveSettingsFragment launchFragment() {
        FragmentManager fragmentManager = mActivity.getSupportFragmentManager();
        TabArchiveSettingsFragment tabArchiveSettingsFragment =
                (TabArchiveSettingsFragment)
                        fragmentManager
                                .getFragmentFactory()
                                .instantiate(
                                        TabArchiveSettingsFragment.class.getClassLoader(),
                                        TabArchiveSettingsFragment.class.getName());
        fragmentManager
                .beginTransaction()
                .replace(android.R.id.content, tabArchiveSettingsFragment)
                .commit();
        mActivityScenario.moveToState(State.STARTED);

        assertEquals(
                mActivity.getString(R.string.archive_settings_title),
                tabArchiveSettingsFragment.getPageTitle().get());
        return tabArchiveSettingsFragment;
    }

    @Test
    @SmallTest
    public void testLaunchSettings() {
        mArchiveSettings.setArchiveEnabled(true);
        mArchiveSettings.setArchiveTimeDeltaDays(7);
        mArchiveSettings.setAutoDeleteEnabled(false);

        TabArchiveSettingsFragment tabArchiveSettingsFragment = launchFragment();

        // Verify the correct radio button is checked.
        TabArchiveTimeDeltaPreference archiveTimeDeltaPreference =
                (TabArchiveTimeDeltaPreference)
                        tabArchiveSettingsFragment.findPreference(
                                TabArchiveSettingsFragment.INACTIVE_TIMEDELTA_PREF);
        assertEquals(
                "After 7 days",
                archiveTimeDeltaPreference.getCheckedRadioButtonForTesting().getPrimaryText());
        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Tabs.ArchiveSettings.TimeDeltaPreference", 0);
        var radioButton = archiveTimeDeltaPreference.getRadioButtonForTesting(0);
        radioButton.onClick(radioButton);
        histogramWatcher.assertExpected();
        assertFalse(mArchiveSettings.getArchiveEnabled());

        histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Tabs.ArchiveSettings.TimeDeltaPreference", 14);
        radioButton = archiveTimeDeltaPreference.getRadioButtonForTesting(2);
        radioButton.onClick(radioButton);
        histogramWatcher.assertExpected();
        assertTrue(mArchiveSettings.getArchiveEnabled());
        assertEquals(14, mArchiveSettings.getArchiveTimeDeltaDays());

        ChromeSwitchPreference enableAutoDelete =
                tabArchiveSettingsFragment.findPreference(
                        TabArchiveSettingsFragment.PREF_TAB_ARCHIVE_ALLOW_AUTODELETE);
        assertFalse(enableAutoDelete.isChecked());

        histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Tabs.ArchiveSettings.AutoDeleteEnabled", true);
        enableAutoDelete.onClick();
        histogramWatcher.assertExpected();
        assertTrue(mArchiveSettings.isAutoDeleteEnabled());

        histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Tabs.ArchiveSettings.AutoDeleteEnabled", false);
        enableAutoDelete.onClick();
        histogramWatcher.assertExpected();
        assertFalse(mArchiveSettings.isAutoDeleteEnabled());
    }
}
