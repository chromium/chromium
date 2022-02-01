// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.preference.Preference;
import androidx.preference.PreferenceCategory;
import androidx.preference.PreferenceFragmentCompat;

import org.chromium.components.browser_ui.settings.ImageButtonPreference;
import org.chromium.components.browser_ui.settings.SettingsUtils;

import java.util.List;

/**
 * Settings fragment for privacy sandbox settings.
 */
public class AdPersonalizationRemovedFragment
        extends PreferenceFragmentCompat implements Preference.OnPreferenceClickListener {
    private static final String TOPICS_CATEGORY_PREFERENCE = "topic_interests";

    private PreferenceCategory mTopicsCategory;

    /**
     * Initializes all the objects related to the preferences page.
     */
    @Override
    public void onCreatePreferences(Bundle bundle, String s) {
        getActivity().setTitle(R.string.privacy_sandbox_remove_interest_title);

        SettingsUtils.addPreferencesFromResource(this, R.xml.ad_personalization_removed_preference);
        mTopicsCategory = findPreference(TOPICS_CATEGORY_PREFERENCE);
        assert mTopicsCategory != null;

        for (String interest : getBlockedTopics()) {
            ImageButtonPreference interestPreference = new ImageButtonPreference(getContext());
            interestPreference.setTitle(interest);
            interestPreference.setImage(
                    R.drawable.ic_add, R.string.privacy_sandbox_add_interest_button_description);
            interestPreference.setDividerAllowedBelow(false);
            interestPreference.setOnPreferenceClickListener(this);
            mTopicsCategory.addPreference(interestPreference);
        }
    }

    @NonNull
    @Override
    public View onCreateView(@NonNull LayoutInflater inflater, @Nullable ViewGroup container,
            @Nullable Bundle savedInstanceState) {
        View view = super.onCreateView(inflater, container, savedInstanceState);
        getListView().setItemAnimator(null);
        return view;
    }

    private List<String> getBlockedTopics() {
        return PrivacySandboxBridge.getBlockedTopics();
    }

    private void allowTopic(String topic) {
        PrivacySandboxBridge.setTopicAllowed(topic, true);
    }

    @Override
    public boolean onPreferenceClick(@NonNull Preference preference) {
        if (preference instanceof ImageButtonPreference) {
            allowTopic(preference.getTitle().toString());
            mTopicsCategory.removePreference(preference);
        }
        return true;
    }
}
