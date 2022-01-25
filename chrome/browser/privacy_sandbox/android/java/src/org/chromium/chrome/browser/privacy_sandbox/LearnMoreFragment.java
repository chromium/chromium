// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import android.os.Bundle;

import androidx.preference.PreferenceFragmentCompat;

import org.chromium.components.browser_ui.settings.SettingsUtils;

/**
 * Settings fragment for privacy sandbox settings.
 */
public class LearnMoreFragment extends PreferenceFragmentCompat {
    /**
     * Initializes all the objects related to the preferences page.
     */
    @Override
    public void onCreatePreferences(Bundle bundle, String s) {
        // TODO(crbug.com/1286276): Replace string placeholders.
        getActivity().setTitle("About personalized ads");
        SettingsUtils.addPreferencesFromResource(this, R.xml.learn_more_preference);
    }
}
