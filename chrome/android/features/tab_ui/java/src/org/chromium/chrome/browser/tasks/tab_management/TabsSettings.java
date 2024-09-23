// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.os.Bundle;

import androidx.annotation.VisibleForTesting;
import androidx.preference.Preference;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.settings.ChromeBaseSettingsFragment;
import org.chromium.chrome.browser.tab.TabArchiveSettings;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncFeatures;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.browser_ui.settings.TextMessagePreference;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;

/** Fragment for tab related configurations to Chrome. */
public class TabsSettings extends ChromeBaseSettingsFragment {
    // Must match key in tabs_settings.xml
    @VisibleForTesting
    static final String PREF_AUTO_OPEN_SYNCED_TAB_GROUPS_SWITCH =
            "auto_open_synced_tab_groups_switch";

    @VisibleForTesting
    static final String PREF_SHOW_TAB_GROUP_CREATION_DIALOG_SWITCH =
            "show_tab_group_creation_dialog_switch";

    @VisibleForTesting
    static final String PREF_TAB_ARCHIVE_SETTINGS = "archive_settings_entrypoint";

    @VisibleForTesting
    static final String PREF_SHARE_TITLES_AND_URLS_WITH_OS_SWITCH =
            "share_titles_and_urls_with_os_switch";

    @VisibleForTesting
    static final String PREF_SHARE_TITLES_AND_URLS_WITH_OS_LEARN_MORE =
            "share_titles_and_urls_with_os_learn_more";

    private final ObservableSupplierImpl<String> mPageTitle = new ObservableSupplierImpl<>();

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        SettingsUtils.addPreferencesFromResource(this, R.xml.tabs_settings);
        mPageTitle.set(getString(R.string.tabs_settings_title));

        configureAutoOpenSyncedTabGroupsSwitch();
        configureShowCreationDialogSwitch();
        configureShareTitlesAndUrlsWithOsSwitch();
    }

    @Override
    public ObservableSupplier<String> getPageTitle() {
        return mPageTitle;
    }

    @Override
    public void onStart() {
        super.onStart();
        configureTabArchiveSettings();
    }

    private void configureAutoOpenSyncedTabGroupsSwitch() {
        ChromeSwitchPreference autoOpenSyncedTabGroupsSwitch =
                (ChromeSwitchPreference) findPreference(PREF_AUTO_OPEN_SYNCED_TAB_GROUPS_SWITCH);
        boolean isTabGroupSyncAutoOpenConfigurable =
                TabGroupSyncFeatures.isTabGroupSyncEnabled(getProfile())
                        && ChromeFeatureList.isEnabled(
                                ChromeFeatureList.TAB_GROUP_SYNC_AUTO_OPEN_KILL_SWITCH);
        if (!isTabGroupSyncAutoOpenConfigurable) {
            autoOpenSyncedTabGroupsSwitch.setVisible(false);
            return;
        }

        PrefService prefService = UserPrefs.get(getProfile());
        boolean isEnabled = prefService.getBoolean(Pref.AUTO_OPEN_SYNCED_TAB_GROUPS);
        autoOpenSyncedTabGroupsSwitch.setChecked(isEnabled);
        autoOpenSyncedTabGroupsSwitch.setOnPreferenceChangeListener(
                (preference, newValue) -> {
                    boolean enabled = (boolean) newValue;
                    prefService.setBoolean(Pref.AUTO_OPEN_SYNCED_TAB_GROUPS, enabled);
                    RecordHistogram.recordBooleanHistogram(
                            "Tabs.AutoOpenSyncedTabGroupsSwitch.ToggledToState", enabled);
                    return true;
                });
    }

    private void configureShowCreationDialogSwitch() {
        ChromeSwitchPreference showTabGroupCreationDialogSwitch =
                (ChromeSwitchPreference) findPreference(PREF_SHOW_TAB_GROUP_CREATION_DIALOG_SWITCH);
        if (!TabUiFeatureUtilities.isTabGroupCreationDialogShowConfigurable()) {
            showTabGroupCreationDialogSwitch.setVisible(false);
            return;
        }

        SharedPreferencesManager prefsManager = ChromeSharedPreferences.getInstance();
        boolean isEnabled =
                prefsManager.readBoolean(ChromePreferenceKeys.SHOW_TAB_GROUP_CREATION_DIALOG, true);
        showTabGroupCreationDialogSwitch.setChecked(isEnabled);
        showTabGroupCreationDialogSwitch.setOnPreferenceChangeListener(
                (preference, newValue) -> {
                    boolean enabled = (boolean) newValue;
                    prefsManager.writeBoolean(
                            ChromePreferenceKeys.SHOW_TAB_GROUP_CREATION_DIALOG, enabled);
                    RecordHistogram.recordBooleanHistogram(
                            "TabGroups.ShowTabGroupCreationDialogSwitch.ToggledToState", enabled);
                    return true;
                });
    }

    private void configureTabArchiveSettings() {
        Preference tabArchiveSettingsPref = findPreference(PREF_TAB_ARCHIVE_SETTINGS);
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.ANDROID_TAB_DECLUTTER)) {
            tabArchiveSettingsPref.setVisible(false);
            return;
        }

        TabArchiveSettings archiveSettings =
                new TabArchiveSettings(ChromeSharedPreferences.getInstance());
        if (archiveSettings.getArchiveEnabled()) {
            int tabArchiveTimeDeltaDays = archiveSettings.getArchiveTimeDeltaDays();
            tabArchiveSettingsPref.setSummary(
                    getResources()
                            .getQuantityString(
                                    R.plurals.archive_settings_summary,
                                    tabArchiveTimeDeltaDays,
                                    tabArchiveTimeDeltaDays));
        } else {
            tabArchiveSettingsPref.setSummary(
                    getResources().getString(R.string.archive_settings_time_delta_never));
        }
        archiveSettings.destroy();
    }

    private void configureShareTitlesAndUrlsWithOsSwitch() {
        ChromeSwitchPreference shareTitlesAndUrlsWithOsSwitch =
                (ChromeSwitchPreference) findPreference(PREF_SHARE_TITLES_AND_URLS_WITH_OS_SWITCH);
        TextMessagePreference learnMoreTextMessagePreference =
                (TextMessagePreference)
                        findPreference(PREF_SHARE_TITLES_AND_URLS_WITH_OS_LEARN_MORE);

        shareTitlesAndUrlsWithOsSwitch.setVisible(false);
        learnMoreTextMessagePreference.setVisible(false);
    }
}
