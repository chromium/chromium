// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;

import androidx.fragment.app.FragmentManager;
import androidx.lifecycle.Lifecycle.State;
import androidx.preference.Preference;
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

import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.TabArchiveSettings;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncFeatures;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncFeaturesJni;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.TextMessagePreference;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.ui.base.TestActivity;

import java.util.concurrent.TimeUnit;

/** Unit tests for {@link TabsSettings}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures({
    ChromeFeatureList.TAB_GROUP_CREATION_DIALOG_ANDROID,
    ChromeFeatureList.TAB_GROUP_PARITY_ANDROID,
    ChromeFeatureList.TAB_GROUP_SYNC_ANDROID,
    ChromeFeatureList.TAB_GROUP_SYNC_AUTO_OPEN_KILL_SWITCH
})
@DisableFeatures(ChromeFeatureList.ANDROID_TAB_DECLUTTER)
public class TabsSettingsUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public JniMocker mJniMocker = new JniMocker();

    private ActivityScenario<TestActivity> mActivityScenario;
    private TestActivity mActivity;
    private SharedPreferencesManager mSharedPreferencesManager;

    @Mock private Profile mProfileMock;
    @Mock private UserPrefs.Natives mUserPrefsJniMock;
    @Mock private PrefService mPrefServiceMock;
    @Mock private TabGroupSyncFeatures.Natives mTabGroupSyncFeaturesJniMock;

    @Before
    public void setUp() {
        mJniMocker.mock(UserPrefsJni.TEST_HOOKS, mUserPrefsJniMock);
        when(mUserPrefsJniMock.get(mProfileMock)).thenReturn(mPrefServiceMock);
        mJniMocker.mock(TabGroupSyncFeaturesJni.TEST_HOOKS, mTabGroupSyncFeaturesJniMock);
        doReturn(true).when(mTabGroupSyncFeaturesJniMock).isTabGroupSyncEnabled(mProfileMock);

        mActivityScenario = ActivityScenario.launch(TestActivity.class);
        mActivityScenario.onActivity(this::onActivity);
        mSharedPreferencesManager = ChromeSharedPreferences.getInstance();
    }

    @After
    public void tearDown() {
        mSharedPreferencesManager.removeKey(ChromePreferenceKeys.SHOW_TAB_GROUP_CREATION_DIALOG);
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

        assertEquals(
                mActivity.getString(R.string.tabs_settings_title),
                tabsSettings.getPageTitle().get());
        return tabsSettings;
    }

    @Test
    @SmallTest
    public void testLaunchTabsSettingsAutoOpenSyncedTabGroupsEnabled() {
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
    public void testLaunchTabsSettingsAutoOpenSyncedTabGroupsDisabled() {
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
        doReturn(false).when(mTabGroupSyncFeaturesJniMock).isTabGroupSyncEnabled(mProfileMock);
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

    @Test
    @SmallTest
    public void testLaunchTabsSettingsGroupCreationDialogEnabled() {
        TabGroupModelFilter.SHOW_TAB_GROUP_CREATION_DIALOG_SETTING.setForTesting(true);

        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "TabGroups.ShowTabGroupCreationDialogSwitch.ToggledToState", false);
        mSharedPreferencesManager.writeBoolean(
                ChromePreferenceKeys.SHOW_TAB_GROUP_CREATION_DIALOG, true);

        TabsSettings tabsSettings = launchFragment();
        ChromeSwitchPreference showTabGroupCreationDialogSwitch =
                tabsSettings.findPreference(
                        TabsSettings.PREF_SHOW_TAB_GROUP_CREATION_DIALOG_SWITCH);
        assertTrue(showTabGroupCreationDialogSwitch.isVisible());
        assertEquals(
                mActivity.getString(R.string.tab_group_creation_dialog_show_setting_text),
                showTabGroupCreationDialogSwitch.getSummary());
        assertTrue(showTabGroupCreationDialogSwitch.isChecked());
        assertTrue(
                mSharedPreferencesManager.readBoolean(
                        ChromePreferenceKeys.SHOW_TAB_GROUP_CREATION_DIALOG, true));

        showTabGroupCreationDialogSwitch.onClick();

        assertFalse(showTabGroupCreationDialogSwitch.isChecked());
        assertFalse(
                mSharedPreferencesManager.readBoolean(
                        ChromePreferenceKeys.SHOW_TAB_GROUP_CREATION_DIALOG, true));
        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    @DisableFeatures(ChromeFeatureList.TAB_GROUP_PARITY_ANDROID)
    public void testTabGroupCreationDialogSettingsHiddenWhenFeatureOff() {
        TabsSettings tabsSettings = launchFragment();
        ChromeSwitchPreference showTabGroupCreationDialogSwitch =
                tabsSettings.findPreference(
                        TabsSettings.PREF_SHOW_TAB_GROUP_CREATION_DIALOG_SWITCH);
        assertFalse(showTabGroupCreationDialogSwitch.isVisible());
    }

    @Test
    @SmallTest
    @DisableFeatures(ChromeFeatureList.ANDROID_TAB_DECLUTTER)
    public void testArchiveSettingsHiddenWhenFeatureOff() {
        TabsSettings tabsSettings = launchFragment();
        Preference archiveSettinsEntryPoint =
                tabsSettings.findPreference(TabsSettings.PREF_TAB_ARCHIVE_SETTINGS);
        assertFalse(archiveSettinsEntryPoint.isVisible());
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.ANDROID_TAB_DECLUTTER)
    public void testArchiveSettingsTitleAndSummary() {
        TabArchiveSettings archiveSettings =
                new TabArchiveSettings(ChromeSharedPreferences.getInstance());
        archiveSettings.setArchiveEnabled(true);
        archiveSettings.setArchiveTimeDeltaHours((int) TimeUnit.DAYS.toHours(14));

        TabsSettings tabsSettings = launchFragment();
        Preference archiveSettinsEntryPoint =
                tabsSettings.findPreference(TabsSettings.PREF_TAB_ARCHIVE_SETTINGS);
        assertTrue(archiveSettinsEntryPoint.isVisible());

        assertEquals("Inactive", archiveSettinsEntryPoint.getTitle());
        assertEquals("After 14 days", archiveSettinsEntryPoint.getSummary());
    }

    @Test
    @SmallTest
    public void testLaunchTabsSettingsShareTitlesAndIrls_noShowWhenDisabled() {
        TabsSettings tabsSettings = launchFragment();
        ChromeSwitchPreference shareTitlesAndUrlsWithOsSwitch =
                tabsSettings.findPreference(TabsSettings.PREF_SHARE_TITLES_AND_URLS_WITH_OS_SWITCH);
        TextMessagePreference learnMoreTextMessagePreference =
                tabsSettings.findPreference(
                        TabsSettings.PREF_SHARE_TITLES_AND_URLS_WITH_OS_LEARN_MORE);
        assertFalse(shareTitlesAndUrlsWithOsSwitch.isVisible());
        assertFalse(learnMoreTextMessagePreference.isVisible());
    }
}
