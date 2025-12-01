// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import android.os.Bundle;
import android.view.View;

import androidx.annotation.StringRes;
import androidx.preference.Preference;
import androidx.preference.PreferenceCategory;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.ChromeManagedPreferenceDelegate;
import org.chromium.chrome.browser.settings.search.BaseSearchIndexProvider;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.ClickableSpansTextMessagePreference;
import org.chromium.components.browser_ui.settings.SettingsFragment;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.browser_ui.settings.TextMessagePreference;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.text.ChromeClickableSpan;
import org.chromium.ui.text.SpanApplier;

import java.util.List;

/** Fragment for the Privacy Sandbox -> Topic preferences. */
@NullMarked
public class TopicsFragment extends PrivacySandboxSettingsBaseFragment
        implements Preference.OnPreferenceChangeListener, Preference.OnPreferenceClickListener {
    private static final String TOPICS_TOGGLE_PREFERENCE = "topics_toggle";
    private static final String TOPICS_EXPLANATION_PREFERENCE = "topics_explanation";
    // private static final String TOPICS_HEADING_PREFERENCE = "topics_heading";
    private static final String CURRENT_TOPICS_PREFERENCE = "current_topics";
    private static final String EMPTY_TOPICS_PREFERENCE = "topics_empty";
    private static final String DISABLED_TOPICS_PREFERENCE = "topics_disabled";
    private static final String TOPICS_PAGE_FOOTER_PREFERENCE = "topics_page_footer";
    private static final String ACTIVE_TOPICS_PREFERENCE = "active_topics";
    private static final String BLOCKED_TOPICS_PREFERENCE = "blocked_topics";
    private static final String MANAGE_TOPICS_PREFERENCE = "manage_topics";
    private static final String TOPICS_DISCLAIMER = "topics_page_disclaimer";

    private ChromeSwitchPreference mTopicsTogglePreference;
    private TextMessagePreference mTopicsExplanationPreference;
    private PreferenceCategory mCurrentTopicsCategory;
    private TextMessagePreference mEmptyTopicsPreference;
    private TextMessagePreference mDisabledTopicsPreference;
    private ClickableSpansTextMessagePreference mTopicsPageFooterPreference;
    private Preference mActiveTopicsPreference;
    private Preference mBlockedTopicsPreference;
    private Preference mManageTopicsPreference;
    private final ObservableSupplierImpl<String> mPageTitle = new ObservableSupplierImpl<>();

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
        mPageTitle.set(getString(R.string.settings_topics_page_title));
        SettingsUtils.addPreferencesFromResource(this, R.xml.topics_preference);

        mTopicsTogglePreference = findPreference(TOPICS_TOGGLE_PREFERENCE);
        mTopicsExplanationPreference = findPreference(TOPICS_EXPLANATION_PREFERENCE);
        mCurrentTopicsCategory = findPreference(CURRENT_TOPICS_PREFERENCE);
        mEmptyTopicsPreference = findPreference(EMPTY_TOPICS_PREFERENCE);
        mDisabledTopicsPreference = findPreference(DISABLED_TOPICS_PREFERENCE);
        mTopicsPageFooterPreference =
                (ClickableSpansTextMessagePreference) findPreference(TOPICS_PAGE_FOOTER_PREFERENCE);
        mActiveTopicsPreference = findPreference(ACTIVE_TOPICS_PREFERENCE);
        mBlockedTopicsPreference = findPreference(BLOCKED_TOPICS_PREFERENCE);
        mManageTopicsPreference = findPreference(MANAGE_TOPICS_PREFERENCE);

        mTopicsTogglePreference.setChecked(isTopicsPrefEnabled(getProfile()));
        mTopicsTogglePreference.setOnPreferenceChangeListener(this);
        mTopicsTogglePreference.setManagedPreferenceDelegate(createManagedPreferenceDelegate());

        mTopicsExplanationPreference.setSummary(
                SpanApplier.applySpans(
                        getResources().getString(R.string.settings_topics_page_disclaimer_clank),
                        new SpanApplier.SpanInfo(
                                "<link1>",
                                "</link1>",
                                new ChromeClickableSpan(
                                        getContext(), this::onManagingAdPrivacyClicked))));
        maybeApplyAdTopicsContentParity();
        maybeApplyAdsApiUxEnhancements();
    }

    @Override
    public ObservableSupplier<String> getPageTitle() {
        return mPageTitle;
    }

    private void maybeApplyAdTopicsContentParity() {
        if (!ChromeFeatureList.isEnabled(
                ChromeFeatureList.PRIVACY_SANDBOX_AD_TOPICS_CONTENT_PARITY)) {
            return;
        }
        mTopicsTogglePreference.setSummary(
                getResources().getString(R.string.settings_ad_topics_page_toggle_sub_label));
        mActiveTopicsPreference.setSummary(
                getResources()
                        .getString(R.string.settings_ad_topics_page_active_topics_description));
    }

    private void maybeApplyAdsApiUxEnhancements() {
        mTopicsPageFooterPreference.setSummary(
                SpanApplier.applySpans(
                        getResources().getString(R.string.settings_ad_topics_page_footer_v2),
                        new SpanApplier.SpanInfo(
                                "<link1>",
                                "</link1>",
                                new ChromeClickableSpan(
                                        getContext(), this::onFledgeSettingsLinkClicked)),
                        new SpanApplier.SpanInfo(
                                "<link2>",
                                "</link2>",
                                new ChromeClickableSpan(
                                        getContext(), this::onCookieSettingsLink))));
        @StringRes int disclaimerStringResId = R.string.settings_ad_topics_page_disclaimer_clank;
        // Use the updated disclaimer text if the Ad Topics Content Parity feature is enabled.
        if (ChromeFeatureList.isEnabled(
                ChromeFeatureList.PRIVACY_SANDBOX_AD_TOPICS_CONTENT_PARITY)) {
            disclaimerStringResId = R.string.settings_ad_topics_page_disclaimer_v2_clank;
        }
        ClickableSpansTextMessagePreference disclaimerPreference =
                findPreference(TOPICS_DISCLAIMER);
        disclaimerPreference.setSummary(
                SpanApplier.applySpans(
                        getResources().getString(disclaimerStringResId),
                        new SpanApplier.SpanInfo(
                                "<link>",
                                "</link>",
                                new ChromeClickableSpan(
                                        getContext(), this::onPrivacyPolicyLinkClicked))));
    }

    private void onManagingAdPrivacyClicked(View unused) {
        getCustomTabLauncher()
                .openUrlInCct(getContext(), PrivacySandboxSettingsFragment.HELP_CENTER_URL);
    }

    private void onFledgeSettingsLinkClicked(View unused) {
        startSettings(FledgeFragment.class);
    }

    private void onCookieSettingsLink(View unused) {
        launchCookieSettings();
    }

    private void onPrivacyPolicyLinkClicked(View unused) {
        RecordUserAction.record("Settings.PrivacySandbox.AdTopics.PrivacyPolicyLinkClicked");
        getCustomTabLauncher()
                .openUrlInCct(
                        getContext(),
                        getPrivacySandboxBridge().shouldUsePrivacyPolicyChinaDomain()
                                ? UrlConstants.GOOGLE_PRIVACY_POLICY_CHINA
                                : UrlConstants.GOOGLE_PRIVACY_POLICY);
    }

    @Override
    public void onViewCreated(View view, @Nullable Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);

        // Disable animations of preference changes.
        getListView().setItemAnimator(null);
    }

    @Override
    public void onStart() {
        super.onStart();
        populateCurrentTopics();
        updatePreferenceVisibility();
    }

    @Override
    public boolean onPreferenceChange(Preference preference, Object value) {
        if (preference.getKey().equals(TOPICS_TOGGLE_PREFERENCE)) {
            boolean enabled = (boolean) value;
            RecordUserAction.record(
                    enabled
                            ? "Settings.PrivacySandbox.Topics.Enabled"
                            : "Settings.PrivacySandbox.Topics.Disabled");
            setTopicsPrefEnabled(getProfile(), enabled);
            updatePreferenceVisibility();
            getPrivacySandboxBridge().topicsToggleChanged(enabled);
            return true;
        }

        return false;
    }

    @Override
    public boolean onPreferenceClick(Preference preference) {
        if (preference instanceof TopicPreference) {
            getPrivacySandboxBridge()
                    .setTopicAllowed(((TopicPreference) preference).getTopic(), false);
            mCurrentTopicsCategory.removePreference(preference);
            updatePreferenceVisibility();

            showSnackbar(
                    R.string.settings_topics_page_block_topic_snackbar,
                    null,
                    Snackbar.TYPE_ACTION,
                    Snackbar.UMA_PRIVACY_SANDBOX_REMOVE_INTEREST,
                    /* actionStringResId= */ 0,
                    /* multiLine= */ true);
            RecordUserAction.record("Settings.PrivacySandbox.Topics.TopicRemoved");
            return true;
        }

        return false;
    }

    private void populateCurrentTopics() {
        mCurrentTopicsCategory.removeAll();
        List<Topic> currentTopics = getPrivacySandboxBridge().getCurrentTopTopics();
        for (Topic topic : currentTopics) {
            TopicPreference preference = new TopicPreference(getContext(), topic);
            preference.setImage(
                    R.drawable.btn_close,
                    getResources()
                            .getString(
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


        // TODO(crbug.com/362973179): Set default values in xml.
        // Always not visible.
        mDisabledTopicsPreference.setVisible(false);

        // Visible when Topics are enabled, but the current Topics list is empty.
        mEmptyTopicsPreference.setVisible(topicsEnabled && topicsEmpty);

        // Visible when Topics are enabled and the current Topics list is not empty.
        mCurrentTopicsCategory.setVisible(topicsEnabled && !topicsEmpty);

        // The new UI hides all the sections when the Topics are disabled.
        mActiveTopicsPreference.setVisible(topicsEnabled);
        mBlockedTopicsPreference.setVisible(topicsEnabled);
        mManageTopicsPreference.setVisible(topicsEnabled);
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

    @Override
    public @SettingsFragment.AnimationType int getAnimationType() {
        return SettingsFragment.AnimationType.PROPERTY;
    }

    public static final BaseSearchIndexProvider SEARCH_INDEX_DATA_PROVIDER =
            new BaseSearchIndexProvider(TopicsFragment.class.getName(), R.xml.topics_preference);
}
