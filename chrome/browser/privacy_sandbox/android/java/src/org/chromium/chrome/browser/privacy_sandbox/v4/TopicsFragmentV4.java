// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox.v4;

import android.os.Bundle;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.preference.Preference;

import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.privacy_sandbox.PrivacySandboxSettingsBaseFragment;
import org.chromium.chrome.browser.privacy_sandbox.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;

/**
 * Fragment for the Privacy Sandbox -> Topic preferences.
 */
public class TopicsFragmentV4 extends PrivacySandboxSettingsBaseFragment
        implements Preference.OnPreferenceChangeListener {
    private static final String TOPICS_TOGGLE_PREFERENCE = "topics_toggle";

    private ChromeSwitchPreference mTopicsTogglePreference;

    static boolean isTopicsPrefEnabled() {
        PrefService prefService = UserPrefs.get(Profile.getLastUsedRegularProfile());
        return prefService.getBoolean(Pref.PRIVACY_SANDBOX_M1_TOPICS_ENABLED);
    }

    static void setTopicsPrefEnabled(boolean isEnabled) {
        PrefService prefService = UserPrefs.get(Profile.getLastUsedRegularProfile());
        prefService.setBoolean(Pref.PRIVACY_SANDBOX_M1_TOPICS_ENABLED, isEnabled);
    }

    @Override
    public void onCreatePreferences(@Nullable Bundle bundle, @Nullable String s) {
        super.onCreatePreferences(bundle, s);
        getActivity().setTitle(R.string.settings_topics_page_title);
        SettingsUtils.addPreferencesFromResource(this, R.xml.topics_preference_v4);

        mTopicsTogglePreference = findPreference(TOPICS_TOGGLE_PREFERENCE);
        mTopicsTogglePreference.setChecked(isTopicsPrefEnabled());
        mTopicsTogglePreference.setOnPreferenceChangeListener(this);
        // TODO(http://b/254411473): Make the preference managed.
    }

    @Override
    public boolean onPreferenceChange(@NonNull Preference preference, Object value) {
        if (preference.getKey().equals(TOPICS_TOGGLE_PREFERENCE)) {
            setTopicsPrefEnabled((boolean) value);
            return true;
        }

        return false;
    }
}
