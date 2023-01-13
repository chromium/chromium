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
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.components.browser_ui.settings.ChromeBasePreference;
import org.chromium.components.browser_ui.settings.FragmentSettingsLauncher;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.favicon.LargeIconBridge;

import java.util.List;

/**
 * Settings fragment for privacy sandbox settings.
 */
public class AdPersonalizationFragment extends PrivacySandboxSettingsBaseFragment
        implements Preference.OnPreferenceClickListener, FragmentSettingsLauncher {
    private static final String AD_PERSONALIZATION_DESCRIPTION = "ad_personalization_description";

    private static final String TOPICS_CATEGORY_PREFERENCE = "topic_interests";
    private static final String EMPTY_TOPICS_PREFERENCE = "empty_topics";
    private static final String REMOVED_TOPICS_PREFERENCE = "removed_topics";

    private static final String FLEDGE_CATEGORY_PREFERENCE = "fledge_interests";
    private static final String EMPTY_FLEDGE_PREFERENCE = "empty_fledge";
    private static final String REMOVED_SITES_PREFERENCE = "removed_sites";

    private PreferenceCategory mTopicsCategory;
    private ChromeBasePreference mEmptyTopicsPreference;
    private Preference mRemovedTopicsPreference;

    private PreferenceCategory mFledgeCategory;
    private ChromeBasePreference mEmptyFledgePreference;
    private Preference mRemovedSitesPreference;

    private Preference mDescriptionPreference;
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
        getActivity().setTitle(R.string.privacy_sandbox_ad_personalization_title);
        SettingsUtils.addPreferencesFromResource(this, R.xml.ad_personalization_preference);

        mDescriptionPreference = findPreference(AD_PERSONALIZATION_DESCRIPTION);
        assert mDescriptionPreference != null;

        mTopicsCategory = findPreference(TOPICS_CATEGORY_PREFERENCE);
        assert mTopicsCategory != null;
        mEmptyTopicsPreference = findPreference(EMPTY_TOPICS_PREFERENCE);
        assert mEmptyTopicsPreference != null;
        mRemovedTopicsPreference = findPreference(REMOVED_TOPICS_PREFERENCE);
        assert mRemovedTopicsPreference != null;

        mFledgeCategory = findPreference(FLEDGE_CATEGORY_PREFERENCE);
        assert mFledgeCategory != null;
        mEmptyFledgePreference = findPreference(EMPTY_FLEDGE_PREFERENCE);
        assert mEmptyFledgePreference != null;
        mRemovedSitesPreference = findPreference(REMOVED_SITES_PREFERENCE);
        assert mRemovedSitesPreference != null;
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

    @Override
    public void onDestroyView() {
        super.onDestroyView();
        if (mLargeIconBridge != null) {
            mLargeIconBridge.destroy();
            mLargeIconBridge = null;
        }
    }

    private void updatePreferences() {
        // Load Fledge data before populating anything. All preferences are hidden initially.
        PrivacySandboxBridge.getFledgeJoiningEtldPlusOneForDisplay(this::populatePreferences);
    }

    private void populatePreferences(List<String> currentFledgeSites) {
        List<Topic> currentTopics = PrivacySandboxBridge.getCurrentTopTopics();
        List<Topic> blockedTopics = PrivacySandboxBridge.getBlockedTopics();
        List<String> blockedFledgeSites =
                PrivacySandboxBridge.getBlockedFledgeJoiningTopFramesForDisplay();

        boolean hasAnyInterests = !(currentTopics.isEmpty() && blockedTopics.isEmpty()
                && currentFledgeSites.isEmpty() && blockedFledgeSites.isEmpty());
        updateDescription(hasAnyInterests);
        populateTopics(currentTopics, blockedTopics);
        populateFledge(currentFledgeSites, blockedFledgeSites);
        updateEmptyState();
    }

    private void updateDescription(boolean hasAnyInterests) {
        int description = PrivacySandboxBridge.isPrivacySandboxEnabled()
                ? (hasAnyInterests
                                ? R.string.privacy_sandbox_ad_personalization_description_trials_on
                                : R.string.privacy_sandbox_ad_personalization_description_no_items)
                : R.string.privacy_sandbox_ad_personalization_description_trials_off;
        mDescriptionPreference.setSummary(description);
    }

    private void populateTopics(List<Topic> currentTopics, List<Topic> blockedTopics) {
        mTopicsCategory.removeAll();
        mTopicsCategory.setVisible(true);
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
        mRemovedTopicsPreference.setVisible(!(currentTopics.isEmpty() && blockedTopics.isEmpty()));
        // If this is the last preference, it shouldn't have a divider below.
        mEmptyTopicsPreference.setDividerAllowedBelow(mRemovedTopicsPreference.isVisible());
    }

    private void populateFledge(List<String> currentFledgeSites, List<String> blockedFledgeSites) {
        mFledgeCategory.removeAll();
        mFledgeCategory.setVisible(true);
        if (mLargeIconBridge == null) {
            mLargeIconBridge = new LargeIconBridge(Profile.getLastUsedRegularProfile());
        }
        for (String site : currentFledgeSites) {
            FledgePreference preference =
                    new FledgePreference(getContext(), site, mLargeIconBridge);
            preference.setImage(R.drawable.btn_close,
                    getResources().getString(
                            R.string.privacy_sandbox_remove_site_button_description, site));
            preference.setDividerAllowedAbove(false);
            preference.setOnPreferenceClickListener(this);
            mFledgeCategory.addPreference(preference);
        }
        mRemovedSitesPreference.setVisible(
                !(currentFledgeSites.isEmpty() && blockedFledgeSites.isEmpty()));
        // If this is the last preference, it shouldn't have a divider below.
        mEmptyFledgePreference.setDividerAllowedBelow(mRemovedSitesPreference.isVisible());
    }

    private void blockTopic(Topic topic) {
        PrivacySandboxBridge.setTopicAllowed(topic, false);
    }

    private void blockFledge(String site) {
        PrivacySandboxBridge.setFledgeJoiningAllowed(site, false);
    }

    @Override
    public boolean onPreferenceClick(@NonNull Preference preference) {
        if (preference instanceof TopicPreference) {
            blockTopic(((TopicPreference) preference).getTopic());
            mTopicsCategory.removePreference(preference);
            showSnackbar(R.string.privacy_sandbox_remove_interest_snackbar, null,
                    Snackbar.TYPE_ACTION, Snackbar.UMA_PRIVACY_SANDBOX_REMOVE_INTEREST);
            RecordUserAction.record("Settings.PrivacySandbox.AdPersonalization.TopicRemoved");
        } else if (preference instanceof FledgePreference) {
            blockFledge(((FledgePreference) preference).getSite());
            mFledgeCategory.removePreference(preference);
            showSnackbar(R.string.privacy_sandbox_remove_site_snackbar, null, Snackbar.TYPE_ACTION,
                    Snackbar.UMA_PRIVACY_SANDBOX_REMOVE_SITE);
            RecordUserAction.record("Settings.PrivacySandbox.AdPersonalization.SiteRemoved");
        } else {
            assert false; // NOTREACHED.
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
            // Override action for the question mark button.
            mSettingsLauncher.launchSettingsActivity(getContext(), LearnMoreFragment.class);
            return true;
        }
        return false;
    }
}
