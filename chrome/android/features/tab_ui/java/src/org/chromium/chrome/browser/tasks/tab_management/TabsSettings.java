// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.os.Bundle;
import android.view.View;

import androidx.annotation.VisibleForTesting;
import androidx.preference.Preference;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.auxiliary_search.AuxiliarySearchConfigManager;
import org.chromium.chrome.browser.auxiliary_search.AuxiliarySearchControllerFactory;
import org.chromium.chrome.browser.auxiliary_search.AuxiliarySearchUtils;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
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
import org.chromium.ui.text.ChromeClickableSpan;
import org.chromium.ui.text.SpanApplier;

/** Fragment for tab related configurations to Chrome. */
@NullMarked
public class TabsSettings extends ChromeBaseSettingsFragment {
    // Must match key in tabs_settings.xml
    @VisibleForTesting
    static final String PREF_AUTO_OPEN_SYNCED_TAB_GROUPS_SWITCH =
            "auto_open_synced_tab_groups_switch";

    @VisibleForTesting
    static final String PREF_TAB_ARCHIVE_SETTINGS = "archive_settings_entrypoint";

    @VisibleForTesting
    static final String PREF_SHARE_TITLES_AND_URLS_WITH_OS_SWITCH =
            "share_titles_and_urls_with_os_switch";

    @VisibleForTesting
    static final String PREF_SHARE_TITLES_AND_URLS_WITH_OS_LEARN_MORE =
            "share_titles_and_urls_with_os_learn_more";

    @VisibleForTesting
    static final String LEARN_MORE_URL = "https://support.google.com/chrome/?p=share_titles_urls";

    private final ObservableSupplierImpl<String> mPageTitle = new ObservableSupplierImpl<>();

    @Override
    public void onCreatePreferences(@Nullable Bundle savedInstanceState, @Nullable String rootKey) {
        SettingsUtils.addPreferencesFromResource(this, R.xml.tabs_settings);
        mPageTitle.set(getString(R.string.tabs_settings_title));

        configureAutoOpenSyncedTabGroupsSwitch();
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

    private void configureTabArchiveSettings() {
        Preference tabArchiveSettingsPref = findPreference(PREF_TAB_ARCHIVE_SETTINGS);

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

        if (!AuxiliarySearchControllerFactory.getInstance().isEnabledAndDeviceCompatible()) {
            shareTitlesAndUrlsWithOsSwitch.setVisible(false);
            learnMoreTextMessagePreference.setVisible(false);
            return;
        }

        boolean isEnabled = AuxiliarySearchUtils.isShareTabsWithOsEnabled();
        shareTitlesAndUrlsWithOsSwitch.setChecked(isEnabled);
        shareTitlesAndUrlsWithOsSwitch.setOnPreferenceChangeListener(
                (preference, newValue) -> {
                    boolean enabled = (boolean) newValue;
                    AuxiliarySearchConfigManager.getInstance().notifyShareTabsStateChanged(enabled);
                    return true;
                });

        learnMoreTextMessagePreference.setSummary(
                SpanApplier.applySpans(
                        getResources()
                                .getString(
                                        R.string
                                                .share_titles_and_urls_with_os_learn_more_setting_text),
                        new SpanApplier.SpanInfo(
                                "<link>",
                                "</link>",
                                new ChromeClickableSpan(getContext(), this::onLearnMoreClicked))));
    }

    @VisibleForTesting
    void onLearnMoreClicked(View view) {
        getCustomTabLauncher().openUrlInCct(getContext(), LEARN_MORE_URL);
    }

    @Override
    public @AnimationType int getAnimationType() {
        return AnimationType.PROPERTY;
    }
}
