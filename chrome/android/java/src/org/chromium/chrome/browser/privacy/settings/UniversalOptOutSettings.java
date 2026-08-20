// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy.settings;

import android.content.Context;
import android.os.Bundle;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.ChromeBaseSettingsFragment;
import org.chromium.chrome.browser.settings.search.ChromeBaseSearchIndexProvider;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.SettingsFragment;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.browser_ui.settings.search.SettingsIndexData;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;

/** Fragment to manage 'Universal Opt Out' preference and to explain to the user what it does. */
@NullMarked
public class UniversalOptOutSettings extends ChromeBaseSettingsFragment {
    @VisibleForTesting
    static final String PREF_UNIVERSAL_OPT_OUT_SWITCH = "universal_opt_out_switch";

    @VisibleForTesting
    static final String PREF_UNIVERSAL_OPT_OUT_INFO_TEXT = "universal_opt_out_info_text";

    private final SettableMonotonicObservableSupplier<String> mPageTitle =
            ObservableSuppliers.createMonotonic();

    @Override
    public void onCreatePreferences(@Nullable Bundle savedInstanceState, @Nullable String rootKey) {
        SettingsUtils.addPreferencesFromResource(this, R.xml.universal_opt_out_preferences);
        mPageTitle.set(getString(R.string.universal_opt_out_sub_page_title));

        ChromeSwitchPreference universalOptOutSwitch =
                findPreference(PREF_UNIVERSAL_OPT_OUT_SWITCH);

        PrefService prefService = UserPrefs.get(getProfile());
        boolean isUniversalOptOutEnabled = prefService.getBoolean(Pref.UNIVERSAL_OPT_OUT_ENABLED);
        universalOptOutSwitch.setChecked(isUniversalOptOutEnabled);

        universalOptOutSwitch.setOnPreferenceChangeListener(
                (preference, newValue) -> {
                    prefService.setBoolean(Pref.UNIVERSAL_OPT_OUT_ENABLED, (boolean) newValue);
                    return true;
                });
    }

    @Override
    public MonotonicObservableSupplier<String> getPageTitle() {
        return mPageTitle;
    }

    @Override
    public @SettingsFragment.AnimationType int getAnimationType() {
        return SettingsFragment.AnimationType.PROPERTY;
    }

    public static boolean shouldShowUniversalOptOutSettings(Profile profile) {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.UNIVERSAL_OPT_OUT_SETTINGS)) {
            return false;
        }

        PrefService userPrefs = UserPrefs.get(profile);
        return userPrefs.getBoolean(Pref.UNIVERSAL_OPT_OUT_ENABLED)
                || userPrefs.getBoolean(Pref.UNIVERSAL_OPT_OUT_ELIGIBLE);
    }

    public static final ChromeBaseSearchIndexProvider SEARCH_INDEX_DATA_PROVIDER =
            new ChromeBaseSearchIndexProvider(
                    UniversalOptOutSettings.class.getName(), R.xml.universal_opt_out_preferences) {
                @Override
                public void updateDynamicPreferences(
                        Context context, SettingsIndexData indexData, Profile profile) {
                    if (!shouldShowUniversalOptOutSettings(profile)) {
                        indexData.removeEntry(getUniqueId(PREF_UNIVERSAL_OPT_OUT_SWITCH));
                        indexData.removeEntry(getUniqueId(PREF_UNIVERSAL_OPT_OUT_INFO_TEXT));
                    }
                }
            };
}
