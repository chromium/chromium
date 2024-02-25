// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync.settings;

import android.os.Bundle;

import androidx.annotation.Nullable;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.settings.ChromeBaseSettingsFragment;
import org.chromium.components.browser_ui.settings.SettingsUtils;

/*
 * Settings fragment containing links to Google Account settings related to Chrome.
 */
public class PersonalizeGoogleServicesSettings extends ChromeBaseSettingsFragment {
    @Override
    public void onCreatePreferences(@Nullable Bundle bundle, @Nullable String s) {
        getActivity().setTitle(R.string.sign_in_personalize_google_services_title);
        SettingsUtils.addPreferencesFromResource(
                this, R.xml.personalize_google_services_preferences);
    }
}
