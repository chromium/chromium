// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import android.os.Bundle;
import android.view.View;

import androidx.annotation.Nullable;
import androidx.preference.Preference;
import androidx.preference.PreferenceCategory;

import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;

import java.util.HashSet;
import java.util.List;

/** Fragment for managing all the topics. */
public class TopicsManageFragment extends PrivacySandboxSettingsBaseFragment {
    private static final String MANAGE_TOPICS_PREFERENCE = "topics_list";

    private PreferenceCategory mTopicsCategory;

    @Override
    public void onCreatePreferences(@Nullable Bundle bundle, @Nullable String s) {
        super.onCreatePreferences(bundle, s);
        getActivity().setTitle(R.string.settings_topics_page_manage_topics_heading);
        SettingsUtils.addPreferencesFromResource(this, R.xml.topics_manage_preference);

        mTopicsCategory = findPreference(MANAGE_TOPICS_PREFERENCE);
        mTopicsCategory.setSummary(
                SpanApplier.applySpans(
                        getResources()
                                .getString(R.string.settings_topics_page_manage_topics_explanation),
                        new SpanApplier.SpanInfo(
                                "<link>",
                                "</link>",
                                new NoUnderlineClickableSpan(
                                        getContext(), this::onLearnMoreClicked))));

        populateTopics();
    }

    private void populateTopics() {
        mTopicsCategory.removeAll();
        List<Topic> firstLevelTopics = PrivacySandboxBridge.getFirstLevelTopics();
        var blockedTopics = new HashSet<Topic>(PrivacySandboxBridge.getBlockedTopics());
        for (Topic topic : firstLevelTopics) {
            var preference = new TopicSwitchPreference(getContext(), topic);
            preference.setChecked(!blockedTopics.contains(topic));
            preference.setOnPreferenceChangeListener(this::onToggleChange);
            mTopicsCategory.addPreference(preference);
        }
    }

    private boolean onToggleChange(Preference preference, Object newValue) {
        var topicPreference = (TopicSwitchPreference) (preference);
        PrivacySandboxBridge.setTopicAllowed(topicPreference.getTopic(), (boolean) newValue);
        return true;
    }

    private void onLearnMoreClicked(View view) {
        openUrlInCct(PrivacySandboxSettingsFragment.HELP_CENTER_URL);
    }
}
