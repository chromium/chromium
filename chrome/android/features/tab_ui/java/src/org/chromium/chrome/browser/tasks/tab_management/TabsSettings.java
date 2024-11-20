// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.provider.Browser;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;
import androidx.browser.customtabs.CustomTabsIntent;
import androidx.preference.Preference;

import org.chromium.base.IntentUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.auxiliary_search.AuxiliarySearchControllerFactory;
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
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;

/** Fragment for tab related configurations to Chrome. */
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

    private final ObservableSupplierImpl<String> mPageTitle = new ObservableSupplierImpl<>();

    static final String LEARN_MORE_URL = "https://support.google.com/chrome/?p=share_titles_urls";

    private CustomTabIntentHelper mCustomTabHelper;

    /**
     * Functional interface to start a Chrome Custom Tab for the given intent, e.g. by using {@link
     * org.chromium.chrome.browser.LaunchIntentDispatcher#createCustomTabActivityIntent}.
     * TODO(crbug.com/40751023): Update when LaunchIntentDispatcher is (partially-)modularized.
     */
    @FunctionalInterface
    public interface CustomTabIntentHelper {
        /**
         * @see org.chromium.chrome.browser.LaunchIntentDispatcher#createCustomTabActivityIntent
         */
        Intent createCustomTabActivityIntent(Context context, Intent intent);
    }

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
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

    /** Set the necessary CCT helpers to be able to natively open links. */
    public void setCustomTabIntentHelper(CustomTabIntentHelper tabHelper) {
        mCustomTabHelper = tabHelper;
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

        if (!AuxiliarySearchControllerFactory.getInstance().isEnabled()) {
            shareTitlesAndUrlsWithOsSwitch.setVisible(false);
            learnMoreTextMessagePreference.setVisible(false);
            return;
        }

        SharedPreferencesManager prefsManager = ChromeSharedPreferences.getInstance();
        boolean isEnabled =
                prefsManager.readBoolean(ChromePreferenceKeys.SHARING_TABS_WITH_OS, true);
        shareTitlesAndUrlsWithOsSwitch.setChecked(isEnabled);
        shareTitlesAndUrlsWithOsSwitch.setOnPreferenceChangeListener(
                (preference, newValue) -> {
                    boolean enabled = (boolean) newValue;
                    prefsManager.writeBoolean(ChromePreferenceKeys.SHARING_TABS_WITH_OS, enabled);
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
                                new NoUnderlineClickableSpan(
                                        getContext(), this::onLearnMoreClicked))));
    }

    @VisibleForTesting
    void onLearnMoreClicked(@NonNull View view) {
        openUrlInCct(LEARN_MORE_URL);
    }

    private void openUrlInCct(String url) {
        assert mCustomTabHelper != null
                : "CCT helpers must be set on TabsSettings before opening a link";
        CustomTabsIntent customTabIntent =
                new CustomTabsIntent.Builder().setShowTitle(true).build();

        customTabIntent.intent.setData(Uri.parse(url));
        Intent intent =
                mCustomTabHelper.createCustomTabActivityIntent(
                        getContext(), customTabIntent.intent);
        intent.setPackage(getContext().getPackageName());
        intent.putExtra(Browser.EXTRA_APPLICATION_ID, getContext().getPackageName());
        IntentUtils.addTrustedIntentExtras(intent);
        IntentUtils.safeStartActivity(getContext(), intent);
    }
}
