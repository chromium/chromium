// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.build.NullUtil.assertNonNull;

import android.content.Context;
import android.os.Bundle;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.ChromeBaseSettingsFragment;
import org.chromium.chrome.browser.settings.search.ChromeBaseSearchIndexProvider;
import org.chromium.chrome.browser.tab.TabArchiveSettings;
import org.chromium.chrome.browser.tab.TabArchiveSettings.Observer;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.browser_ui.settings.search.SettingsIndexData;
import org.chromium.components.browser_ui.settings.search.SettingsIndexData.Entry;

/** Fragment for tab archive configurations to Chrome. */
@NullMarked
public class TabArchiveSettingsFragment extends ChromeBaseSettingsFragment {
    // Must match key in tab_archive_settings.xml
    static final String PREF_TAB_ARCHIVE_SETTINGS_DESCRIPTION = "tab_archive_settings_description";
    static final String PREF_TAB_ARCHIVE_ALLOW_AUTODELETE = "tab_archive_allow_autodelete";
    static final String INACTIVE_TIMEDELTA_PREF = "tab_archive_time_delta";
    static final String PREF_TAB_ARCHIVE_INCLUDE_DUPLICATE_TABS =
            "tab_archive_include_duplicate_tabs";

    private final TabArchiveSettings.Observer mTabArchiveSettingsObserver =
            new Observer() {
                @Override
                public void onSettingChanged() {
                    configureSettings();
                }
            };

    private TabArchiveSettings mArchiveSettings;

    private final SettableMonotonicObservableSupplier<String> mPageTitle =
            ObservableSuppliers.createMonotonic();

    @Override
    public void onCreatePreferences(@Nullable Bundle savedInstanceState, @Nullable String rootKey) {
        mArchiveSettings = new TabArchiveSettings(ChromeSharedPreferences.getInstance());
        mArchiveSettings.addObserver(mTabArchiveSettingsObserver);
        SettingsUtils.addPreferencesFromResource(this, R.xml.tab_archive_settings);
        mPageTitle.set(getString(R.string.archive_settings_title));

        configureSettings();
    }

    @Override
    public MonotonicObservableSupplier<String> getPageTitle() {
        return mPageTitle;
    }

    @Override
    public void onDestroy() {
        super.onDestroy();

        if (mArchiveSettings != null) {
            mArchiveSettings.removeObserver(mTabArchiveSettingsObserver);
        }
    }

    private void configureSettings() {
        // Archive time delta radio button.
        TabArchiveTimeDeltaPreference archiveTimeDeltaPreference =
                (TabArchiveTimeDeltaPreference) findPreference(INACTIVE_TIMEDELTA_PREF);
        archiveTimeDeltaPreference.initialize(mArchiveSettings);

        // Auto delete switch.
        ChromeSwitchPreference enableAutoDeleteSwitch =
                (ChromeSwitchPreference) findPreference(PREF_TAB_ARCHIVE_ALLOW_AUTODELETE);
        int autoDeleteTimeDeltaMonths = mArchiveSettings.getAutoDeleteTimeDeltaMonths();
        enableAutoDeleteSwitch.setSummary(
                getResources()
                        .getQuantityString(
                                R.plurals.archive_settings_allow_autodelete_summary,
                                autoDeleteTimeDeltaMonths,
                                autoDeleteTimeDeltaMonths));
        boolean isAutoDeleteEnabled =
                mArchiveSettings.getArchiveEnabled() && mArchiveSettings.isAutoDeleteEnabled();
        enableAutoDeleteSwitch.setEnabled(mArchiveSettings.getArchiveEnabled());
        enableAutoDeleteSwitch.setChecked(isAutoDeleteEnabled);
        enableAutoDeleteSwitch.setOnPreferenceChangeListener(
                (preference, newValue) -> {
                    boolean enabled = (boolean) newValue;
                    mArchiveSettings.setAutoDeleteEnabled(enabled);
                    RecordHistogram.recordBooleanHistogram(
                            "Tabs.ArchiveSettings.AutoDeleteEnabled", enabled);
                    return true;
                });

        // Duplicate tabs switch.
        ChromeSwitchPreference enableArchiveDuplicateTabsSwitch =
                (ChromeSwitchPreference) findPreference(PREF_TAB_ARCHIVE_INCLUDE_DUPLICATE_TABS);
        enableArchiveDuplicateTabsSwitch.setEnabled(mArchiveSettings.getArchiveEnabled());
        enableArchiveDuplicateTabsSwitch.setChecked(
                mArchiveSettings.isArchiveDuplicateTabsEnabled());
        enableArchiveDuplicateTabsSwitch.setOnPreferenceChangeListener(
                (preference, newValue) -> {
                    boolean enabled = (boolean) newValue;
                    mArchiveSettings.setArchiveDuplicateTabsEnabled(enabled);
                    RecordHistogram.recordBooleanHistogram(
                            "Tabs.ArchiveSettings.ArchiveDuplicateTabsEnabled", enabled);
                    return true;
                });
    }

    @Override
    public @AnimationType int getAnimationType() {
        return AnimationType.PROPERTY;
    }

    public static final ChromeBaseSearchIndexProvider SEARCH_INDEX_DATA_PROVIDER =
            new ChromeBaseSearchIndexProvider(
                    TabArchiveSettingsFragment.class.getName(), R.xml.tab_archive_settings) {
                @Override
                public void updateDynamicPreferences(
                        Context context, SettingsIndexData indexData, Profile profile) {
                    // In the layout, PREF_TAB_ARCHIVE_SETTINGS_DESCRIPTION is meant to be a
                    // description of the INACTIVE_TIMEDELTA_PREF. As for Dec 2025, these two
                    // preferences are list as independent items in the settings xml, and there's no
                    // meaningful title being associated with them.
                    // As a result, we remove the text pref from the searchable index, and use the
                    // literal string as summary for the radio button.
                    indexData.removeEntry(getUniqueId(PREF_TAB_ARCHIVE_SETTINGS_DESCRIPTION));

                    String idInactiveTimeDelta = getUniqueId(INACTIVE_TIMEDELTA_PREF);
                    Entry prevEntry = assertNonNull(indexData.getEntry(idInactiveTimeDelta));
                    // LINT.IfChange(archive_settings_description_section)
                    Entry newEntry =
                            new Entry.Builder(prevEntry)
                                    .setSummary(
                                            context.getString(
                                                    R.string.archive_settings_description_section))
                                    .build();
                    // LINT.ThenChange(/chrome/android/features/tab_ui/java/res/xml/tab_archive_settings.xml:archive_settings_description_section)

                    indexData.updateEntry(idInactiveTimeDelta, newEntry);
                }
            };
}
