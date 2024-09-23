// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.os.Bundle;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.settings.ChromeBaseSettingsFragment;
import org.chromium.chrome.browser.tab.TabArchiveSettings;
import org.chromium.chrome.browser.tab.TabArchiveSettings.Observer;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.SettingsUtils;

/** Fragment for tab archive configurations to Chrome. */
public class TabArchiveSettingsFragment extends ChromeBaseSettingsFragment {
    // Must match key in tab_archive_settings.xml
    static final String PREF_TAB_ARCHIVE_ALLOW_AUTODELETE = "tab_archive_allow_autodelete";
    static final String INACTIVE_TIMEDELTA_PREF = "tab_archive_time_delta";

    private final TabArchiveSettings.Observer mTabArchiveSettingsObserver =
            new Observer() {
                @Override
                public void onSettingChanged() {
                    configureSettings();
                }
            };

    private TabArchiveSettings mArchiveSettings;

    private final ObservableSupplierImpl<String> mPageTitle = new ObservableSupplierImpl<>();

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        mArchiveSettings = new TabArchiveSettings(ChromeSharedPreferences.getInstance());
        mArchiveSettings.addObserver(mTabArchiveSettingsObserver);
        SettingsUtils.addPreferencesFromResource(this, R.xml.tab_archive_settings);
        mPageTitle.set(getString(R.string.archive_settings_title));

        configureSettings();
    }

    @Override
    public ObservableSupplier<String> getPageTitle() {
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
        int autoDeleteTimeDeltaDays = mArchiveSettings.getAutoDeleteTimeDeltaDays();
        enableAutoDeleteSwitch.setTitle(
                getResources()
                        .getQuantityString(
                                R.plurals.archive_settings_allow_autodelete_title,
                                autoDeleteTimeDeltaDays,
                                autoDeleteTimeDeltaDays));
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
    }
}
