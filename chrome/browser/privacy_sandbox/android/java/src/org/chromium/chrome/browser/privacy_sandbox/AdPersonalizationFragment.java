// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import android.os.Bundle;

import androidx.annotation.NonNull;
import androidx.preference.Preference;
import androidx.preference.PreferenceCategory;
import androidx.preference.PreferenceFragmentCompat;

import org.chromium.components.browser_ui.settings.ImageButtonPreference;
import org.chromium.components.browser_ui.settings.SettingsUtils;

import java.util.List;

/**
 * Settings fragment for privacy sandbox settings.
 */
public class AdPersonalizationFragment
        extends PreferenceFragmentCompat implements Preference.OnPreferenceClickListener {
    private static final String TOPICS_CATEGORY_PREFERENCE = "topic_interests";

    private PreferenceCategory mTopicsCategory;

    /**
     * Initializes all the objects related to the preferences page.
     */
    @Override
    public void onCreatePreferences(Bundle bundle, String s) {
        getActivity().setTitle(R.string.privacy_sandbox_ad_personalization_title);

        SettingsUtils.addPreferencesFromResource(this, R.xml.ad_personalization_preference);
        mTopicsCategory = findPreference(TOPICS_CATEGORY_PREFERENCE);
        assert mTopicsCategory != null;

        ImageButtonPreference lastPreference = null;
        for (String interest : getTopicInterests()) {
            ImageButtonPreference interestPreference = new ImageButtonPreference(getContext());
            lastPreference = interestPreference;
            interestPreference.setTitle(interest);
            interestPreference.setImage(R.drawable.btn_close,
                    R.string.privacy_sandbox_remove_interest_button_description);
            interestPreference.setDividerAllowedBelow(false);
            interestPreference.setOnPreferenceClickListener(this);
            mTopicsCategory.addPreference(interestPreference);
        }
        if (lastPreference != null) {
            lastPreference.setDividerAllowedBelow(true);
        }
    }

    private List<String> getTopicInterests() {
        return PrivacySandboxBridge.getCurrentTopTopics();
    }

    private void removeTopicInterest(String topic) {
        PrivacySandboxBridge.setTopicAllowed(topic, false);
    }

    @Override
    public boolean onPreferenceClick(@NonNull Preference preference) {
        if (preference instanceof ImageButtonPreference) {
            removeTopicInterest(preference.getTitle().toString());
            mTopicsCategory.removePreference(preference);
        }
        return true;
    }
}
