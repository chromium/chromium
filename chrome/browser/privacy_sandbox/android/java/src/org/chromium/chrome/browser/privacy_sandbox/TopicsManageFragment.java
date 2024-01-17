// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import android.os.Bundle;
import android.view.View;

import androidx.annotation.Nullable;
import androidx.preference.PreferenceCategory;

import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;

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
        List<Topic> topics = PrivacySandboxBridge.getFirstLevelTopics();
        for (Topic topic : topics) {
            mTopicsCategory.addPreference(new TopicSwitchPreference(getContext(), topic));
        }
    }

    private void onLearnMoreClicked(View view) {
        openUrlInCct(PrivacySandboxSettingsFragment.HELP_CENTER_URL);
    }
}
