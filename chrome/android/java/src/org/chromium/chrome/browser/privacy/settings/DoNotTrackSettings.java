// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy.settings;

import android.os.Bundle;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.build.NullUtil;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.settings.ChromeBaseSettingsFragment;
import org.chromium.chrome.browser.settings.search.BaseSearchIndexProvider;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.SettingsFragment;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.browser_ui.settings.TextMessagePreference;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;

/** Fragment to manage 'Do Not Track' preference and to explain to the user what it does. */
@NullMarked
public class DoNotTrackSettings extends ChromeBaseSettingsFragment {
    // Must match key in do_not_track_preferences.xml.
    private static final String PREF_DO_NOT_TRACK_SWITCH = "do_not_track_switch";
    private static final String PREF_DO_NOT_TRACK_DESCRIPTION = "do_not_track_description";

    private final ObservableSupplierImpl<String> mPageTitle = new ObservableSupplierImpl<>();

    @Override
    public void onCreatePreferences(@Nullable Bundle savedInstanceState, @Nullable String rootKey) {
        SettingsUtils.addPreferencesFromResource(this, R.xml.do_not_track_preferences);
        mPageTitle.set(getString(R.string.do_not_track_title));

        ChromeSwitchPreference doNotTrackSwitch =
                (ChromeSwitchPreference) findPreference(PREF_DO_NOT_TRACK_SWITCH);

        PrefService prefService = UserPrefs.get(getProfile());
        boolean isDoNotTrackEnabled = prefService.getBoolean(Pref.ENABLE_DO_NOT_TRACK);
        doNotTrackSwitch.setChecked(isDoNotTrackEnabled);

        doNotTrackSwitch.setOnPreferenceChangeListener(
                (preference, newValue) -> {
                    prefService.setBoolean(Pref.ENABLE_DO_NOT_TRACK, (boolean) newValue);
                    return true;
                });

        if (ChromeFeatureList.sAndroidSettingsContainment.isEnabled()) {
            // TODO(crbug.com/439911511): Set the summary instead of the title in the layout file.
            TextMessagePreference doNotTrackDescription =
                    findPreference(PREF_DO_NOT_TRACK_DESCRIPTION);
            NullUtil.assertNonNull(doNotTrackDescription)
                    .setSummary(doNotTrackDescription.getTitle());
            doNotTrackDescription.setTitle(null);
        }
    }

    @Override
    public ObservableSupplier<String> getPageTitle() {
        return mPageTitle;
    }

    @Override
    public @SettingsFragment.AnimationType int getAnimationType() {
        return SettingsFragment.AnimationType.PROPERTY;
    }

    public static final BaseSearchIndexProvider SEARCH_INDEX_DATA_PROVIDER =
            new BaseSearchIndexProvider(
                    DoNotTrackSettings.class.getName(), R.xml.do_not_track_preferences);
}
