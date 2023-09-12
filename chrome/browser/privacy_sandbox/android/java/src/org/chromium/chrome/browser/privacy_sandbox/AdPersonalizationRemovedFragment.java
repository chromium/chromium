// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.preference.Preference;
import androidx.preference.PreferenceCategory;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.components.browser_ui.settings.FragmentSettingsLauncher;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.favicon.LargeIconBridge;

/**
 * Settings fragment for privacy sandbox settings.
 */
public class AdPersonalizationRemovedFragment extends PrivacySandboxSettingsBaseFragment
        implements Preference.OnPreferenceClickListener, FragmentSettingsLauncher {
    private static final String TOPICS_CATEGORY_PREFERENCE = "topic_interests";
    private static final String EMPTY_TOPICS_PREFERENCE = "empty_topics";
    private static final String FLEDGE_CATEGORY_PREFERENCE = "fledge_interests";
    private static final String EMPTY_FLEDGE_PREFERENCE = "empty_fledge";

    private PreferenceCategory mTopicsCategory;
    private Preference mEmptyTopicsPreference;
    private PreferenceCategory mFledgeCategory;
    private Preference mEmptyFledgePreference;
    private LargeIconBridge mLargeIconBridge;
    private SettingsLauncher mSettingsLauncher;

    @Override
    public void setSettingsLauncher(SettingsLauncher settingsLauncher) {
        mSettingsLauncher = settingsLauncher;
    }

    /**
     * Initializes all the objects related to the preferences page.
     */
    @Override
    public void onCreatePreferences(Bundle bundle, String s) {
        assert (!ChromeFeatureList.isEnabled(ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS_4));

        super.onCreatePreferences(bundle, s);
        getActivity().setTitle(R.string.privacy_sandbox_remove_interest_title);

        SettingsUtils.addPreferencesFromResource(this, R.xml.ad_personalization_removed_preference);
        mTopicsCategory = findPreference(TOPICS_CATEGORY_PREFERENCE);
        assert mTopicsCategory != null;
        mEmptyTopicsPreference = findPreference(EMPTY_TOPICS_PREFERENCE);
        assert mEmptyTopicsPreference != null;

        mFledgeCategory = findPreference(FLEDGE_CATEGORY_PREFERENCE);
        assert mFledgeCategory != null;
        mEmptyFledgePreference = findPreference(EMPTY_FLEDGE_PREFERENCE);
        assert mEmptyFledgePreference != null;

        if (mLargeIconBridge == null) {
            mLargeIconBridge = new LargeIconBridge(getProfile());
        }

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
        for (String site : PrivacySandboxBridge.getBlockedFledgeJoiningTopFramesForDisplay()) {
            FledgePreference preference =
                    new FledgePreference(getContext(), site, mLargeIconBridge);
            preference.setImage(R.drawable.ic_add,
                    getResources().getString(
                            R.string.privacy_sandbox_add_site_button_description, site));
            preference.setDividerAllowedBelow(false);
            preference.setOnPreferenceClickListener(this);
            mFledgeCategory.addPreference(preference);
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

    @Override
    public void onDestroyView() {
        super.onDestroyView();
        if (mLargeIconBridge != null) {
            mLargeIconBridge.destroy();
            mLargeIconBridge = null;
        }
    }

    private void allowTopic(Topic topic) {
        PrivacySandboxBridge.setTopicAllowed(topic, true);
    }

    private void allowFledge(String site) {
        PrivacySandboxBridge.setFledgeJoiningAllowed(site, true);
    }

    @Override
    public boolean onPreferenceClick(@NonNull Preference preference) {
        if (preference instanceof TopicPreference) {
            allowTopic(((TopicPreference) preference).getTopic());
            mTopicsCategory.removePreference(preference);
            showSnackbar(R.string.privacy_sandbox_add_interest_snackbar, null, Snackbar.TYPE_ACTION,
                    Snackbar.UMA_PRIVACY_SANDBOX_ADD_INTEREST);
            RecordUserAction.record("Settings.PrivacySandbox.RemovedInterests.TopicAdded");
        } else if (preference instanceof FledgePreference) {
            allowFledge(((FledgePreference) preference).getSite());
            mFledgeCategory.removePreference(preference);
            showSnackbar(R.string.privacy_sandbox_add_site_snackbar, null, Snackbar.TYPE_ACTION,
                    Snackbar.UMA_PRIVACY_SANDBOX_ADD_SITE);
            RecordUserAction.record("Settings.PrivacySandbox.RemovedInterests.SiteAdded");
        } else {
            assert false; // NOTREACHED
        }
        updateEmptyState();
        return true;
    }

    private void updateEmptyState() {
        mEmptyTopicsPreference.setVisible(mTopicsCategory.getPreferenceCount() == 0);
        mEmptyFledgePreference.setVisible(mFledgeCategory.getPreferenceCount() == 0);
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        if (item.getItemId() == R.id.menu_id_targeted_help) {
            // Override action for the help button.
            mSettingsLauncher.launchSettingsActivity(getContext(), LearnMoreFragment.class);
            return true;
        }
        return false;
    }
}
