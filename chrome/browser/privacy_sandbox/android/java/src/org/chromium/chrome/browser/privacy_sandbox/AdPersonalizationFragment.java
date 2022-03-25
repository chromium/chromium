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

import java.util.List;

/**
 * Settings fragment for privacy sandbox settings.
 */
public class AdPersonalizationFragment
        extends PreferenceFragmentCompat implements Preference.OnPreferenceClickListener {
    private static final String AD_PERSONALIZATION_DESCRIPTION = "ad_personalization_description";
    private static final String TOPICS_CATEGORY_PREFERENCE = "topic_interests";
    private static final String EMPTY_TOPICS_PREFERENCE = "empty_topics";
    private static final String REMOVE_TOPICS_PREFERENCE = "removed_topics";

    private SnackbarManager mSnackbarManager;

    private PreferenceCategory mTopicsCategory;
    private Preference mEmptyTopicsPreference;
    private Preference mRemoveTopicsPreference;
    private Preference mDescriptionPreference;

    public void setSnackbarManager(SnackbarManager snackbarManager) {
        mSnackbarManager = snackbarManager;
    }

    /**
     * Initializes all the objects related to the preferences page.
     */
    @Override
    public void onCreatePreferences(Bundle bundle, String s) {
        getActivity().setTitle(R.string.privacy_sandbox_ad_personalization_title);
        SettingsUtils.addPreferencesFromResource(this, R.xml.ad_personalization_preference);

        mDescriptionPreference = findPreference(AD_PERSONALIZATION_DESCRIPTION);
        assert mDescriptionPreference != null;
        mTopicsCategory = findPreference(TOPICS_CATEGORY_PREFERENCE);
        assert mTopicsCategory != null;
        mEmptyTopicsPreference = findPreference(EMPTY_TOPICS_PREFERENCE);
        assert mEmptyTopicsPreference != null;
        mRemoveTopicsPreference = findPreference(REMOVE_TOPICS_PREFERENCE);
        assert mRemoveTopicsPreference != null;
    }

    @Override
    public void onResume() {
        updatePreferences();
        super.onResume();
    }

    @NonNull
    @Override
    public View onCreateView(@NonNull LayoutInflater inflater, @Nullable ViewGroup container,
            @Nullable Bundle savedInstanceState) {
        View view = super.onCreateView(inflater, container, savedInstanceState);
        getListView().setItemAnimator(null);
        return view;
    }

    private void updatePreferences() {
        List<Topic> currentTopics = PrivacySandboxBridge.getCurrentTopTopics();
        List<Topic> blockedTopics = PrivacySandboxBridge.getBlockedTopics();

        int description = PrivacySandboxBridge.isPrivacySandboxEnabled()
                ? (currentTopics.isEmpty() && blockedTopics.isEmpty()
                                ? R.string.privacy_sandbox_ad_personalization_description_no_items
                                : R.string.privacy_sandbox_ad_personalization_description_trials_on)
                : R.string.privacy_sandbox_ad_personalization_description_trials_off;
        mDescriptionPreference.setSummary(description);

        mTopicsCategory.removeAll();
        for (Topic topic : currentTopics) {
            TopicPreference preference = new TopicPreference(getContext(), topic);
            preference.setImage(R.drawable.btn_close,
                    getResources().getString(
                            R.string.privacy_sandbox_remove_interest_button_description,
                            topic.getName()));
            preference.setDividerAllowedAbove(false);
            preference.setOnPreferenceClickListener(this);
            mTopicsCategory.addPreference(preference);
        }
        updateEmptyState();
        mRemoveTopicsPreference.setVisible(!currentTopics.isEmpty() || !blockedTopics.isEmpty());
    }

    private void blockTopic(Topic topic) {
        PrivacySandboxBridge.setTopicAllowed(topic, false);
    }

    @Override
    public boolean onPreferenceClick(@NonNull Preference preference) {
        if (preference instanceof TopicPreference) {
            blockTopic(((TopicPreference) preference).getTopic());
            mTopicsCategory.removePreference(preference);
            updateEmptyState();
            mSnackbarManager.showSnackbar(Snackbar.make(
                    getResources().getString(R.string.privacy_sandbox_remove_interest_snackbar),
                    null, Snackbar.TYPE_ACTION, Snackbar.UMA_PRIVACY_SANDBOX_REMOVE_INTEREST));
            RecordUserAction.record("Settings.PrivacySandbox.AdPersonalization.TopicRemoved");
        }
        return true;
    }

    private void updateEmptyState() {
        mEmptyTopicsPreference.setVisible(mTopicsCategory.getPreferenceCount() == 0);
    }
}
