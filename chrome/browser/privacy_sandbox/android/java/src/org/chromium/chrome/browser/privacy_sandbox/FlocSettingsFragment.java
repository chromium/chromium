// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;

import androidx.preference.PreferenceFragmentCompat;

import org.chromium.components.browser_ui.settings.SettingsUtils;

/**
 * Settings fragment for FLoC settings as part of Privacy Sandbox.
 */
public class FlocSettingsFragment extends PreferenceFragmentCompat {
    public static final String FLOC_DESCRIPTION = "floc_description";
    public static final String FLOC_STATUS = "floc_status";
    public static final String FLOC_GROUP = "floc_group";
    public static final String FLOC_UPDATE = "floc_update";

    /**
     * Initializes all the objects related to the preferences page.
     */
    @Override
    public void onCreatePreferences(Bundle bundle, String s) {
        // Add all preferences and set the title.
        getActivity().setTitle(R.string.prefs_privacy_sandbox_floc);
        SettingsUtils.addPreferencesFromResource(this, R.xml.floc_preferences);
        findPreference(FLOC_DESCRIPTION).setSummary(R.string.privacy_sandbox_floc_description);
        updateInformation();
    }

    @Override
    public View onCreateView(
            LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        LinearLayout view =
                (LinearLayout) super.onCreateView(inflater, container, savedInstanceState);
        // Add a button to the bottom of the preferences view.
        LinearLayout buttonView =
                (LinearLayout) inflater.inflate(R.layout.floc_button, view, false);
        view.addView(buttonView);
        return view;
    }

    private void updateInformation() {
        findPreference(FLOC_STATUS)
                .setSummary(getContext().getString(R.string.privacy_sandbox_floc_status_title)
                        + "\n" + PrivacySandboxBridge.getFlocStatusString());
        findPreference(FLOC_GROUP)
                .setSummary(getContext().getString(R.string.privacy_sandbox_floc_group_title) + "\n"
                        + PrivacySandboxBridge.getFlocGroupString());
        findPreference(FLOC_UPDATE).setSummary(R.string.privacy_sandbox_floc_update_title);
    }
}
