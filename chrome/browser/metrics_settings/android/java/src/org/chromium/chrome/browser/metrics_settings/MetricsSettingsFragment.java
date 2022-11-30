// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.metrics_settings;

import android.os.Bundle;

import androidx.preference.PreferenceFragmentCompat;

import org.chromium.components.browser_ui.settings.SettingsUtils;

/**
 * Settings fragment for metrics. This class represents a View in the MVC paradigm.
 */
public class MetricsSettingsFragment extends PreferenceFragmentCompat {
    /**
     * Initializes all the objects related to the preferences page.
     */
    @Override
    public void onCreatePreferences(Bundle bundle, String s) {
        // Add all preferences and set the title.
        getActivity().setTitle(R.string.prefs_metrics_settings);
        SettingsUtils.addPreferencesFromResource(this, R.xml.metrics_preferences);
    }
}
