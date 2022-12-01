// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox.v4;

import android.os.Bundle;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.preference.Preference;
import androidx.preference.PreferenceCategory;

import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.privacy_sandbox.PrivacySandboxSettingsBaseFragment;
import org.chromium.chrome.browser.privacy_sandbox.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.settings.ChromeBasePreference;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.browser_ui.settings.TextMessagePreference;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;

/**
 * Fragment for the Privacy Sandbox -> Topic preferences.
 */
public class TopicsFragmentV4 extends PrivacySandboxSettingsBaseFragment
        implements Preference.OnPreferenceChangeListener {
    private static final String TOPICS_TOGGLE_PREFERENCE = "topics_toggle";
    private static final String CURRENT_TOPICS_PREFERENCE = "current_topics";
    private static final String EMPTY_TOPICS_PREFERENCE = "topics_empty";
    private static final String DISABLED_TOPICS_PREFERENCE = "topics_disabled";
    private static final String BLOCKED_TOPICS_PREFERENCE = "blocked_topics";
    private static final String TOPICS_PAGE_FOOTER_PREFERENCE = "topics_page_footer";

    private ChromeSwitchPreference mTopicsTogglePreference;
    private PreferenceCategory mCurrentTopicsPreference;
    private TextMessagePreference mEmptyTopicsPreference;
    private TextMessagePreference mDisabledTopicsPreference;
    private ChromeBasePreference mBlockedTopicsPreference;
    private TextMessagePreference mTopicsPageFooterPreference;

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
        mCurrentTopicsPreference = findPreference(CURRENT_TOPICS_PREFERENCE);
        mEmptyTopicsPreference = findPreference(EMPTY_TOPICS_PREFERENCE);
        mDisabledTopicsPreference = findPreference(DISABLED_TOPICS_PREFERENCE);
        mBlockedTopicsPreference = findPreference(BLOCKED_TOPICS_PREFERENCE);
        mTopicsPageFooterPreference = findPreference(TOPICS_PAGE_FOOTER_PREFERENCE);

        mTopicsTogglePreference.setChecked(isTopicsPrefEnabled());
        mTopicsTogglePreference.setOnPreferenceChangeListener(this);
        // TODO(http://b/254411473): Make the preference managed.
    }

    @Override
    public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);

        // Disable animations of preference changes.
        getListView().setItemAnimator(null);
    }

    @Override
    public void onResume() {
        super.onResume();
        updatePreferenceVisibility();
    }

    @Override
    public boolean onPreferenceChange(@NonNull Preference preference, Object value) {
        if (preference.getKey().equals(TOPICS_TOGGLE_PREFERENCE)) {
            setTopicsPrefEnabled((boolean) value);
            updatePreferenceVisibility();
            return true;
        }

        return false;
    }

    private void updatePreferenceVisibility() {
        if (!isTopicsPrefEnabled()) {
            mDisabledTopicsPreference.setVisible(true);

            mCurrentTopicsPreference.setVisible(false);
            mEmptyTopicsPreference.setVisible(false);
            mTopicsPageFooterPreference.setVisible(false);
            mBlockedTopicsPreference.setDividerAllowedBelow(false);
            return;
        }

        if (mCurrentTopicsPreference.getPreferenceCount() == 0) {
            mEmptyTopicsPreference.setVisible(true);

            mCurrentTopicsPreference.setVisible(false);
            mDisabledTopicsPreference.setVisible(false);
            mTopicsPageFooterPreference.setVisible(false);
            mBlockedTopicsPreference.setDividerAllowedBelow(false);
            return;
        }
    }
}
