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

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.settings.SettingsUtils;

/**
 * Settings fragment for privacy sandbox settings.
 */
public class AdPersonalizationRemovedFragment
        extends PreferenceFragmentCompat implements Preference.OnPreferenceClickListener {
    private static final String TOPICS_CATEGORY_PREFERENCE = "topic_interests";
    private static final String EMPTY_TOPICS_PREFERENCE = "empty_topics";

    private PreferenceCategory mTopicsCategory;
    private SnackbarManager mSnackbarManager;
    private Preference mEmptyTopicsPreference;

    public void setSnackbarManager(SnackbarManager snackbarManager) {
        mSnackbarManager = snackbarManager;
    }

    /**
     * Initializes all the objects related to the preferences page.
     */
    @Override
    public void onCreatePreferences(Bundle bundle, String s) {
        getActivity().setTitle(R.string.privacy_sandbox_remove_interest_title);

        SettingsUtils.addPreferencesFromResource(this, R.xml.ad_personalization_removed_preference);
        mTopicsCategory = findPreference(TOPICS_CATEGORY_PREFERENCE);
        assert mTopicsCategory != null;
        mEmptyTopicsPreference = findPreference(EMPTY_TOPICS_PREFERENCE);
        assert mEmptyTopicsPreference != null;

        for (Topic topic : PrivacySandboxBridge.getBlockedTopics()) {
            TopicPreference preference = new TopicPreference(getContext(), topic);
            preference.setImage(R.drawable.ic_add,
                    getResources().getString(
                            R.string.privacy_sandbox_add_interest_button_description,
                            topic.getName()));
            preference.setDividerAllowedBelow(false);
            preference.setOnPreferenceClickListener(this);
            mTopicsCategory.addPreference(preference);
        }
        updateEmptyState();
    }

    @NonNull
    @Override
    public View onCreateView(@NonNull LayoutInflater inflater, @Nullable ViewGroup container,
            @Nullable Bundle savedInstanceState) {
        View view = super.onCreateView(inflater, container, savedInstanceState);
        getListView().setItemAnimator(null);
        return view;
    }

    private void allowTopic(Topic topic) {
        PrivacySandboxBridge.setTopicAllowed(topic, true);
    }

    @Override
    public boolean onPreferenceClick(@NonNull Preference preference) {
        if (preference instanceof TopicPreference) {
            allowTopic(((TopicPreference) preference).getTopic());
            mTopicsCategory.removePreference(preference);
            updateEmptyState();
            mSnackbarManager.showSnackbar(Snackbar.make(
                    getResources().getString(R.string.privacy_sandbox_add_interest_snackbar), null,
                    Snackbar.TYPE_ACTION, Snackbar.UMA_PRIVACY_SANDBOX_ADD_INTEREST));
            RecordUserAction.record("Settings.PrivacySandbox.RemovedInterests.TopicAdded");
        }
        return true;
    }

    private void updateEmptyState() {
        mEmptyTopicsPreference.setVisible(mTopicsCategory.getPreferenceCount() == 0);
    }
}
