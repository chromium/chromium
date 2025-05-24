// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.Context;
import android.view.View;

import androidx.fragment.app.FragmentManager;
import androidx.lifecycle.Lifecycle.State;
import androidx.preference.Preference;
import androidx.test.core.app.ActivityScenario;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.auxiliary_search.AuxiliarySearchConfigManager;
import org.chromium.chrome.browser.auxiliary_search.AuxiliarySearchControllerFactory;
import org.chromium.chrome.browser.auxiliary_search.AuxiliarySearchHooks;
import org.chromium.chrome.browser.auxiliary_search.AuxiliarySearchUtils;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.TabArchiveSettings;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncFeatures;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncFeaturesJni;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.SettingsCustomTabLauncher;
import org.chromium.components.browser_ui.settings.TextMessagePreference;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.ui.base.TestActivity;

import java.util.concurrent.TimeUnit;

/** Unit tests for {@link TabsSettings}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures({
    ChromeFeatureList.TAB_GROUP_SYNC_ANDROID,
    ChromeFeatureList.TAB_GROUP_SYNC_AUTO_OPEN_KILL_SWITCH
})
public class TabsSettingsUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    private ActivityScenario<TestActivity> mActivityScenario;
    private TestActivity mActivity;

    @Mock private Profile mProfileMock;
    @Mock private UserPrefs.Natives mUserPrefsJniMock;
    @Mock private PrefService mPrefServiceMock;
    @Mock private TabGroupSyncFeatures.Natives mTabGroupSyncFeaturesJniMock;
    @Mock private SettingsCustomTabLauncher mCustomTabLauncher;

    @Before
    public void setUp() {
        UserPrefsJni.setInstanceForTesting(mUserPrefsJniMock);
        when(mUserPrefsJniMock.get(mProfileMock)).thenReturn(mPrefServiceMock);
        TabGroupSyncFeaturesJni.setInstanceForTesting(mTabGroupSyncFeaturesJniMock);
        doReturn(true).when(mTabGroupSyncFeaturesJniMock).isTabGroupSyncEnabled(mProfileMock);
        doNothing().when(mCustomTabLauncher).openUrlInCct(any(Context.class), anyString());

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
        tabsSettings.setCustomTabLauncher(mCustomTabLauncher);
        fragmentManager.beginTransaction().replace(android.R.id.content, tabsSettings).commit();
        mActivityScenario.moveToState(State.STARTED);

        assertEquals(
                mActivity.getString(R.string.tabs_settings_title),
                tabsSettings.getPageTitle().get());
        return tabsSettings;
    }

    @Test
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
    @DisableFeatures(ChromeFeatureList.TAB_GROUP_SYNC_ANDROID)
    public void testTabGroupSyncSettingsHiddenWhenFeatureOff() {
        doReturn(false).when(mTabGroupSyncFeaturesJniMock).isTabGroupSyncEnabled(mProfileMock);
        TabsSettings tabsSettings = launchFragment();
        ChromeSwitchPreference autoOpenSyncedTabGroupsSwitch =
                tabsSettings.findPreference(TabsSettings.PREF_AUTO_OPEN_SYNCED_TAB_GROUPS_SWITCH);
        assertFalse(autoOpenSyncedTabGroupsSwitch.isVisible());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.TAB_GROUP_SYNC_AUTO_OPEN_KILL_SWITCH)
    public void testTabGroupSyncSettingsHiddenWhenKillswitchEnabled() {
        TabsSettings tabsSettings = launchFragment();
        ChromeSwitchPreference autoOpenSyncedTabGroupsSwitch =
                tabsSettings.findPreference(TabsSettings.PREF_AUTO_OPEN_SYNCED_TAB_GROUPS_SWITCH);
        assertFalse(autoOpenSyncedTabGroupsSwitch.isVisible());
    }

    @Test
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
    public void testLaunchTabsSettingsShareTabs_noShowWhenDisabled() {
        TabsSettings tabsSettings = launchFragment();
        ChromeSwitchPreference shareTitlesAndUrlsWithOsSwitch =
                tabsSettings.findPreference(TabsSettings.PREF_SHARE_TITLES_AND_URLS_WITH_OS_SWITCH);
        TextMessagePreference learnMoreTextMessagePreference =
                tabsSettings.findPreference(
                        TabsSettings.PREF_SHARE_TITLES_AND_URLS_WITH_OS_LEARN_MORE);
        assertFalse(shareTitlesAndUrlsWithOsSwitch.isVisible());
        assertFalse(learnMoreTextMessagePreference.isVisible());
    }

    @Test
    public void testLaunchTabsSettingsShareTabs_NotShowWhenDeviceNotCompatible() {
        AuxiliarySearchHooks hooksMock = Mockito.mock(AuxiliarySearchHooks.class);
        when(hooksMock.isEnabled()).thenReturn(true);
        when(hooksMock.isSettingDefaultEnabledByOs()).thenReturn(true);
        AuxiliarySearchControllerFactory.getInstance().setHooksForTesting(hooksMock);
        // Sets no consumer schema exists.
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.AUXILIARY_SEARCH_CONSUMER_SCHEMA_FOUND, false);

        TabsSettings tabsSettings = launchFragment();
        ChromeSwitchPreference shareTitlesAndUrlsWithOsSwitch =
                tabsSettings.findPreference(TabsSettings.PREF_SHARE_TITLES_AND_URLS_WITH_OS_SWITCH);
        TextMessagePreference learnMoreTextMessagePreference =
                tabsSettings.findPreference(
                        TabsSettings.PREF_SHARE_TITLES_AND_URLS_WITH_OS_LEARN_MORE);
        assertFalse(shareTitlesAndUrlsWithOsSwitch.isVisible());
        assertFalse(learnMoreTextMessagePreference.isVisible());
    }

    @Test
    public void testLaunchTabsSettingsShareTabs() {
        AuxiliarySearchHooks hooksMock = Mockito.mock(AuxiliarySearchHooks.class);
        when(hooksMock.isEnabled()).thenReturn(true);
        when(hooksMock.isSettingDefaultEnabledByOs()).thenReturn(true);
        AuxiliarySearchControllerFactory.getInstance().setHooksForTesting(hooksMock);
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.AUXILIARY_SEARCH_CONSUMER_SCHEMA_FOUND, true);
        assertTrue(AuxiliarySearchControllerFactory.getInstance().isSettingDefaultEnabledByOs());
        assertTrue(AuxiliarySearchUtils.isShareTabsWithOsEnabled());

        TabsSettings tabsSettings = launchFragment();
        ChromeSwitchPreference shareTitlesAndUrlsWithOsSwitch =
                tabsSettings.findPreference(TabsSettings.PREF_SHARE_TITLES_AND_URLS_WITH_OS_SWITCH);
        TextMessagePreference learnMoreTextMessagePreference =
                tabsSettings.findPreference(
                        TabsSettings.PREF_SHARE_TITLES_AND_URLS_WITH_OS_LEARN_MORE);
        assertTrue(shareTitlesAndUrlsWithOsSwitch.isVisible());
        assertTrue(learnMoreTextMessagePreference.isVisible());

        var listener =
                Mockito.mock(AuxiliarySearchConfigManager.ShareTabsWithOsStateListener.class);
        AuxiliarySearchConfigManager.getInstance().addListener(listener);
        shareTitlesAndUrlsWithOsSwitch.onClick();

        assertFalse(shareTitlesAndUrlsWithOsSwitch.isChecked());
        verify(listener).onConfigChanged(eq(false));
        AuxiliarySearchConfigManager.getInstance().removeListener(listener);
    }

    @Test
    public void testLaunchTabsSettingsShareTabs_DefaultDisabled() {
        AuxiliarySearchHooks hooksMock = Mockito.mock(AuxiliarySearchHooks.class);
        when(hooksMock.isEnabled()).thenReturn(true);
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.AUXILIARY_SEARCH_CONSUMER_SCHEMA_FOUND, true);
        // Sets the setting as default disabled.
        when(hooksMock.isSettingDefaultEnabledByOs()).thenReturn(false);
        AuxiliarySearchControllerFactory.getInstance().setHooksForTesting(hooksMock);
        assertFalse(AuxiliarySearchUtils.isShareTabsWithOsEnabled());

        TabsSettings tabsSettings = launchFragment();
        ChromeSwitchPreference shareTitlesAndUrlsWithOsSwitch =
                tabsSettings.findPreference(TabsSettings.PREF_SHARE_TITLES_AND_URLS_WITH_OS_SWITCH);
        TextMessagePreference learnMoreTextMessagePreference =
                tabsSettings.findPreference(
                        TabsSettings.PREF_SHARE_TITLES_AND_URLS_WITH_OS_LEARN_MORE);
        assertTrue(shareTitlesAndUrlsWithOsSwitch.isVisible());
        assertFalse(shareTitlesAndUrlsWithOsSwitch.isChecked());
        assertTrue(learnMoreTextMessagePreference.isVisible());

        var listener =
                Mockito.mock(AuxiliarySearchConfigManager.ShareTabsWithOsStateListener.class);
        AuxiliarySearchConfigManager.getInstance().addListener(listener);
        shareTitlesAndUrlsWithOsSwitch.onClick();

        assertTrue(shareTitlesAndUrlsWithOsSwitch.isChecked());
        verify(listener).onConfigChanged(eq(true));
        AuxiliarySearchConfigManager.getInstance().removeListener(listener);
    }

    @Test
    public void testLaunchTabsSettingsShareTabs_LearnMore() {
        AuxiliarySearchHooks hooksMock = Mockito.mock(AuxiliarySearchHooks.class);
        when(hooksMock.isEnabled()).thenReturn(true);
        when(hooksMock.isSettingDefaultEnabledByOs()).thenReturn(true);
        AuxiliarySearchControllerFactory.getInstance().setHooksForTesting(hooksMock);
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.AUXILIARY_SEARCH_CONSUMER_SCHEMA_FOUND, true);

        TabsSettings tabsSettings = launchFragment();
        ChromeSwitchPreference shareTitlesAndUrlsWithOsSwitch =
                tabsSettings.findPreference(TabsSettings.PREF_SHARE_TITLES_AND_URLS_WITH_OS_SWITCH);
        TextMessagePreference learnMoreTextMessagePreference =
                tabsSettings.findPreference(
                        TabsSettings.PREF_SHARE_TITLES_AND_URLS_WITH_OS_LEARN_MORE);
        assertTrue(shareTitlesAndUrlsWithOsSwitch.isVisible());
        assertTrue(learnMoreTextMessagePreference.isVisible());

        View view = Mockito.mock(View.class);
        tabsSettings.onLearnMoreClicked(view);
        verify(mCustomTabLauncher).openUrlInCct(eq(mActivity), eq(TabsSettings.LEARN_MORE_URL));
    }
}
