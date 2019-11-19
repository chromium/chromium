// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences;

import android.os.Bundle;
import android.support.v7.preference.Preference;
import android.support.v7.preference.PreferenceFragmentCompat;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.FeatureUtilities;
import org.chromium.chrome.browser.partnercustomizations.HomepageManager;

/**
 * Fragment that allows the user to configure homepage related preferences.
 */
public class HomepagePreferences extends PreferenceFragmentCompat {
    @VisibleForTesting
    public static final String PREF_HOMEPAGE_SWITCH = "homepage_switch";
    private static final String PREF_HOMEPAGE_EDIT = "homepage_edit";

    private HomepageManager mHomepageManager;
    private Preference mHomepageEdit;

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        mHomepageManager = HomepageManager.getInstance();

        getActivity().setTitle(R.string.options_homepage_title);
        PreferenceUtils.addPreferencesFromResource(this, R.xml.homepage_preferences);

        ChromeSwitchPreference homepageSwitch =
                (ChromeSwitchPreference) findPreference(PREF_HOMEPAGE_SWITCH);

        if (FeatureUtilities.isBottomToolbarEnabled()) {
            homepageSwitch.setVisible(false);
        } else {
            boolean isHomepageEnabled = mHomepageManager.getPrefHomepageEnabled();
            homepageSwitch.setChecked(isHomepageEnabled);
            homepageSwitch.setOnPreferenceChangeListener((preference, newValue) -> {
                mHomepageManager.setPrefHomepageEnabled((boolean) newValue);
                return true;
            });
        }

        mHomepageEdit = findPreference(PREF_HOMEPAGE_EDIT);
        updateCurrentHomepageUrl();
    }

    private void updateCurrentHomepageUrl() {
        mHomepageEdit.setSummary(mHomepageManager.getPrefHomepageUseDefaultUri()
                        ? HomepageManager.getDefaultHomepageUri()
                        : mHomepageManager.getPrefHomepageCustomUri());
    }

    @Override
    public void onResume() {
        super.onResume();
        updateCurrentHomepageUrl();
    }
}
