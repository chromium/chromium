// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

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
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.ui.base.TestActivity;

/** Unit tests for {@link TabsSettings}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures({
    ChromeFeatureList.TAB_GROUP_SYNC_ANDROID,
    ChromeFeatureList.TAB_GROUP_SYNC_AUTO_OPEN_KILL_SWITCH
})
public class TabsSettingsUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public JniMocker mJniMocker = new JniMocker();

    private ActivityScenario<TestActivity> mActivityScenario;
    private TestActivity mActivity;
    private TabsSettings mTabsSettings;

    @Mock private Profile mProfileMock;
    @Mock private UserPrefs.Natives mUserPrefsJniMock;
    @Mock private PrefService mPrefServiceMock;

    @Before
    public void setUp() {
        mJniMocker.mock(UserPrefsJni.TEST_HOOKS, mUserPrefsJniMock);
        when(mUserPrefsJniMock.get(mProfileMock)).thenReturn(mPrefServiceMock);

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

    private TabsSettings launchFragment() {
        FragmentManager fragmentManager = mActivity.getSupportFragmentManager();
        TabsSettings tabsSettings =
                (TabsSettings)
                        fragmentManager
                                .getFragmentFactory()
                                .instantiate(
                                        TabsSettings.class.getClassLoader(),
                                        TabsSettings.class.getName());
        tabsSettings.setProfile(mProfileMock);
        fragmentManager.beginTransaction().replace(android.R.id.content, tabsSettings).commit();
        mActivityScenario.moveToState(State.STARTED);

        assertEquals(mActivity.getString(R.string.tabs_settings_title), mActivity.getTitle());
        return tabsSettings;
    }

    @Test
    @SmallTest
    public void testLaunchTabsSettingsAutoOpenSynedTabGroupsEnabled() {
        when(mPrefServiceMock.getBoolean(Pref.AUTO_OPEN_SYNCED_TAB_GROUPS)).thenReturn(true);

        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Tabs.AutoOpenSyncedTabGroupsSwitch.ToggledToState", false);
        TabsSettings tabsSettings = launchFragment();
        ChromeSwitchPreference autoOpenSyncedTabGroupsSwitch =
                tabsSettings.findPreference(TabsSettings.PREF_AUTO_OPEN_SYNCED_TAB_GROUPS_SWITCH);
        assertEquals(
                mActivity.getString(R.string.auto_open_synced_tab_groups_summary),
                autoOpenSyncedTabGroupsSwitch.getSummary());
        assertTrue(autoOpenSyncedTabGroupsSwitch.isChecked());

        autoOpenSyncedTabGroupsSwitch.onClick();

        assertFalse(autoOpenSyncedTabGroupsSwitch.isChecked());
        verify(mPrefServiceMock).setBoolean(Pref.AUTO_OPEN_SYNCED_TAB_GROUPS, false);
        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testLaunchTabsSettingsAutoOpenSynedTabGroupsDisabled() {
        when(mPrefServiceMock.getBoolean(Pref.AUTO_OPEN_SYNCED_TAB_GROUPS)).thenReturn(false);

        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Tabs.AutoOpenSyncedTabGroupsSwitch.ToggledToState", true);
        TabsSettings tabsSettings = launchFragment();
        ChromeSwitchPreference autoOpenSyncedTabGroupsSwitch =
                tabsSettings.findPreference(TabsSettings.PREF_AUTO_OPEN_SYNCED_TAB_GROUPS_SWITCH);
        assertEquals(
                mActivity.getString(R.string.auto_open_synced_tab_groups_summary),
                autoOpenSyncedTabGroupsSwitch.getSummary());
        assertFalse(autoOpenSyncedTabGroupsSwitch.isChecked());

        autoOpenSyncedTabGroupsSwitch.onClick();

        assertTrue(autoOpenSyncedTabGroupsSwitch.isChecked());
        verify(mPrefServiceMock).setBoolean(Pref.AUTO_OPEN_SYNCED_TAB_GROUPS, true);
        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    @DisableFeatures(ChromeFeatureList.TAB_GROUP_SYNC_ANDROID)
    public void testTabGroupSyncSettingsHiddenWhenFeatureOff() {
        TabsSettings tabsSettings = launchFragment();
        ChromeSwitchPreference autoOpenSyncedTabGroupsSwitch =
                tabsSettings.findPreference(TabsSettings.PREF_AUTO_OPEN_SYNCED_TAB_GROUPS_SWITCH);
        assertFalse(autoOpenSyncedTabGroupsSwitch.isVisible());
    }

    @Test
    @SmallTest
    @DisableFeatures(ChromeFeatureList.TAB_GROUP_SYNC_AUTO_OPEN_KILL_SWITCH)
    public void testTabGroupSyncSettingsHiddenWhenKillswitchEnabled() {
        TabsSettings tabsSettings = launchFragment();
        ChromeSwitchPreference autoOpenSyncedTabGroupsSwitch =
                tabsSettings.findPreference(TabsSettings.PREF_AUTO_OPEN_SYNCED_TAB_GROUPS_SWITCH);
        assertFalse(autoOpenSyncedTabGroupsSwitch.isVisible());
    }
}
