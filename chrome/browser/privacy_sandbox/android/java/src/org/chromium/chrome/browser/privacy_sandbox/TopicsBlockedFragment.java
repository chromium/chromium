// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import android.os.Bundle;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.preference.Preference;
import androidx.preference.PreferenceCategory;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.components.browser_ui.settings.SettingsUtils;

import java.util.List;

/** Fragment for the blocked Topic preferences. */
public class TopicsBlockedFragment extends PrivacySandboxSettingsBaseFragment
        implements Preference.OnPreferenceClickListener {
    private static final String BLOCKED_TOPICS_PREFERENCE = "block_list";

    private PreferenceCategory mBlockedTopicsCategory;

    @Override
    public void onCreatePreferences(@Nullable Bundle bundle, @Nullable String s) {
        super.onCreatePreferences(bundle, s);
        getActivity().setTitle(R.string.settings_topics_page_blocked_topics_sub_page_title);
        SettingsUtils.addPreferencesFromResource(this, R.xml.block_list_preference);

        mBlockedTopicsCategory = findPreference(BLOCKED_TOPICS_PREFERENCE);
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
        populateBlockedTopics();
        updateBlockedTopicsDescription();
    }

    @Override
    public boolean onPreferenceClick(@NonNull Preference preference) {
        if (preference instanceof TopicPreference) {
            PrivacySandboxBridge.setTopicAllowed(((TopicPreference) preference).getTopic(), true);
            mBlockedTopicsCategory.removePreference(preference);
            updateBlockedTopicsDescription();

            showSnackbar(
                    R.string.settings_topics_page_add_topic_snackbar,
                    null,
                    Snackbar.TYPE_ACTION,
                    Snackbar.UMA_PRIVACY_SANDBOX_ADD_INTEREST);
            RecordUserAction.record("Settings.PrivacySandbox.Topics.TopicAdded");
            return true;
        }
        return false;
    }

    private void populateBlockedTopics() {
        mBlockedTopicsCategory.removeAll();
        List<Topic> blockedTopics = PrivacySandboxBridge.getBlockedTopics();
        for (Topic topic : blockedTopics) {
            TopicPreference preference = new TopicPreference(getContext(), topic);
            preference.setImage(
                    R.drawable.ic_add,
                    getResources()
                            .getString(
                                    R.string.privacy_sandbox_add_interest_button_description,
                                    topic.getName()));
            preference.setDividerAllowedBelow(false);
            preference.setOnPreferenceClickListener(this);
            mBlockedTopicsCategory.addPreference(preference);
        }
    }

    private void updateBlockedTopicsDescription() {
        mBlockedTopicsCategory.setSummary(
                mBlockedTopicsCategory.getPreferenceCount() == 0
                        ? R.string.settings_topics_page_blocked_topics_description_empty
                        : R.string.settings_topics_page_blocked_topics_description);
    }
}
