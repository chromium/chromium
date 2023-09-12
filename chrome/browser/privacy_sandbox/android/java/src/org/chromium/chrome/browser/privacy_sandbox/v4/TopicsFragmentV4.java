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

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.privacy_sandbox.PrivacySandboxBridge;
import org.chromium.chrome.browser.privacy_sandbox.PrivacySandboxSettingsBaseFragment;
import org.chromium.chrome.browser.privacy_sandbox.R;
import org.chromium.chrome.browser.privacy_sandbox.Topic;
import org.chromium.chrome.browser.privacy_sandbox.TopicPreference;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.ChromeManagedPreferenceDelegate;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.ClickableSpansTextMessagePreference;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.browser_ui.settings.TextMessagePreference;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;

import java.util.List;

/**
 * Fragment for the Privacy Sandbox -> Topic preferences.
 */
public class TopicsFragmentV4 extends PrivacySandboxSettingsBaseFragment
        implements Preference.OnPreferenceChangeListener, Preference.OnPreferenceClickListener {
    private static final String TOPICS_TOGGLE_PREFERENCE = "topics_toggle";
    private static final String TOPICS_HEADING_PREFERENCE = "topics_heading";
    private static final String CURRENT_TOPICS_PREFERENCE = "current_topics";
    private static final String EMPTY_TOPICS_PREFERENCE = "topics_empty";
    private static final String DISABLED_TOPICS_PREFERENCE = "topics_disabled";
    private static final String TOPICS_PAGE_FOOTER_PREFERENCE = "topics_page_footer";

    private ChromeSwitchPreference mTopicsTogglePreference;
    private PreferenceCategoryWithClickableSummary mTopicsHeadingPreference;
    private PreferenceCategory mCurrentTopicsCategory;
    private TextMessagePreference mEmptyTopicsPreference;
    private TextMessagePreference mDisabledTopicsPreference;
    private ClickableSpansTextMessagePreference mTopicsPageFooterPreference;

    static boolean isTopicsPrefEnabled(Profile profile) {
        PrefService prefService = UserPrefs.get(profile);
        return prefService.getBoolean(Pref.PRIVACY_SANDBOX_M1_TOPICS_ENABLED);
    }

    static void setTopicsPrefEnabled(Profile profile, boolean isEnabled) {
        PrefService prefService = UserPrefs.get(profile);
        prefService.setBoolean(Pref.PRIVACY_SANDBOX_M1_TOPICS_ENABLED, isEnabled);
    }

    static boolean isTopicsPrefManaged(Profile profile) {
        PrefService prefService = UserPrefs.get(profile);
        return prefService.isManagedPreference(Pref.PRIVACY_SANDBOX_M1_TOPICS_ENABLED);
    }

    @Override
    public void onCreatePreferences(@Nullable Bundle bundle, @Nullable String s) {
        super.onCreatePreferences(bundle, s);
        getActivity().setTitle(R.string.settings_topics_page_title);
        SettingsUtils.addPreferencesFromResource(this, R.xml.topics_preference_v4);

        mTopicsTogglePreference = findPreference(TOPICS_TOGGLE_PREFERENCE);
        mTopicsHeadingPreference = findPreference(TOPICS_HEADING_PREFERENCE);
        mCurrentTopicsCategory = findPreference(CURRENT_TOPICS_PREFERENCE);
        mEmptyTopicsPreference = findPreference(EMPTY_TOPICS_PREFERENCE);
        mDisabledTopicsPreference = findPreference(DISABLED_TOPICS_PREFERENCE);
        mTopicsPageFooterPreference =
                (ClickableSpansTextMessagePreference) findPreference(TOPICS_PAGE_FOOTER_PREFERENCE);

        mTopicsTogglePreference.setChecked(isTopicsPrefEnabled(getProfile()));
        mTopicsTogglePreference.setOnPreferenceChangeListener(this);
        mTopicsTogglePreference.setManagedPreferenceDelegate(createManagedPreferenceDelegate());

        mTopicsHeadingPreference.setSummary(SpanApplier.applySpans(
                getResources().getString(R.string.settings_topics_page_current_topics_description),
                new SpanApplier.SpanInfo("<link>", "</link>",
                        new NoUnderlineClickableSpan(getContext(), this::onLearnMoreClicked))));

        mTopicsPageFooterPreference.setSummary(SpanApplier.applySpans(
                getResources().getString(R.string.settings_topics_page_footer),
                new SpanApplier.SpanInfo("<link1>", "</link1>",
                        new NoUnderlineClickableSpan(
                                getContext(), this::onFledgeSettingsLinkClicked)),
                new SpanApplier.SpanInfo("<link2>", "</link2>",
                        new NoUnderlineClickableSpan(getContext(), this::onCookieSettingsLink))));
    }

    private void onLearnMoreClicked(View view) {
        RecordUserAction.record("Settings.PrivacySandbox.Topics.LearnMoreClicked");
        launchSettingsActivity(TopicsLearnMoreFragment.class);
    }

    private void onFledgeSettingsLinkClicked(View view) {
        launchSettingsActivity(FledgeFragmentV4.class);
    }

    private void onCookieSettingsLink(View view) {
        launchCookieSettings();
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
        populateCurrentTopics();
        updatePreferenceVisibility();
    }

    @Override
    public boolean onPreferenceChange(@NonNull Preference preference, Object value) {
        if (preference.getKey().equals(TOPICS_TOGGLE_PREFERENCE)) {
            boolean enabled = (boolean) value;
            RecordUserAction.record(enabled ? "Settings.PrivacySandbox.Topics.Enabled"
                                            : "Settings.PrivacySandbox.Topics.Disabled");
            setTopicsPrefEnabled(getProfile(), enabled);
            updatePreferenceVisibility();
            PrivacySandboxBridge.topicsToggleChanged(enabled);
            return true;
        }

        return false;
    }

    @Override
    public boolean onPreferenceClick(@NonNull Preference preference) {
        if (preference instanceof TopicPreference) {
            PrivacySandboxBridge.setTopicAllowed(((TopicPreference) preference).getTopic(), false);
            mCurrentTopicsCategory.removePreference(preference);
            updatePreferenceVisibility();

            showSnackbar(R.string.settings_topics_page_block_topic_snackbar, null,
                    Snackbar.TYPE_ACTION, Snackbar.UMA_PRIVACY_SANDBOX_REMOVE_INTEREST);
            RecordUserAction.record("Settings.PrivacySandbox.Topics.TopicRemoved");
            return true;
        }

        return false;
    }

    private void populateCurrentTopics() {
        mCurrentTopicsCategory.removeAll();
        List<Topic> currentTopics = PrivacySandboxBridge.getCurrentTopTopics();
        for (Topic topic : currentTopics) {
            TopicPreference preference = new TopicPreference(getContext(), topic);
            preference.setImage(R.drawable.btn_close,
                    getResources().getString(
                            R.string.privacy_sandbox_remove_interest_button_description,
                            topic.getName()));
            preference.setDividerAllowedAbove(false);
            preference.setOnPreferenceClickListener(this);
            mCurrentTopicsCategory.addPreference(preference);
        }
    }

    private void updatePreferenceVisibility() {
        boolean topicsEnabled = isTopicsPrefEnabled(getProfile());
        boolean topicsEmpty = mCurrentTopicsCategory.getPreferenceCount() == 0;

        // Visible when Topics are disabled.
        mDisabledTopicsPreference.setVisible(!topicsEnabled);

        // Visible when Topics are enabled, but the current Topics list is empty.
        mEmptyTopicsPreference.setVisible(topicsEnabled && topicsEmpty);

        // Visible when Topics are enabled and the current Topics list is not empty.
        mCurrentTopicsCategory.setVisible(topicsEnabled && !topicsEmpty);
    }

    private ChromeManagedPreferenceDelegate createManagedPreferenceDelegate() {
        return new ChromeManagedPreferenceDelegate(getProfile()) {
            @Override
            public boolean isPreferenceControlledByPolicy(Preference preference) {
                if (TOPICS_TOGGLE_PREFERENCE.equals(preference.getKey())) {
                    return isTopicsPrefManaged(getProfile());
                }
                return false;
            }
        };
    }
}
