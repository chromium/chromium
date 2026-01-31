// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.os.Bundle;

import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.search.ChromeBaseSearchIndexProvider;
import org.chromium.components.browser_ui.settings.ChromeBasePreference;
import org.chromium.components.browser_ui.settings.SettingsFragment;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.browser_ui.settings.search.SettingsIndexData;

/** Settings fragment for privacy sandbox settings. */
@NullMarked
public class PrivacySandboxSettingsFragment extends PrivacySandboxSettingsBaseFragment {
    public static final String TOPICS_PREF = "topics";
    public static final String FLEDGE_PREF = "fledge";
    public static final String AD_MEASUREMENT_PREF = "ad_measurement";
    public static final String HELP_CENTER_URL = "https://support.google.com/chrome/?p=ad_privacy";

    private @Nullable ChromeBasePreference mTopicsPref;
    private @Nullable ChromeBasePreference mFledgePref;

    private ChromeBasePreference mAdMeasurementPref;
    private final SettableMonotonicObservableSupplier<String> mPageTitle =
            ObservableSuppliers.createMonotonic();

    @Override
    public void onCreatePreferences(@Nullable Bundle bundle, @Nullable String s) {
        super.onCreatePreferences(bundle, s);

        // This view should not be shown when PS is restricted, unless the
        // isRestrictedNoticeEnabled flag is enabled.
        assert !getPrivacySandboxBridge().isPrivacySandboxRestricted()
                || getPrivacySandboxBridge().isRestrictedNoticeEnabled();

        // Add all preferences and set the title
        mPageTitle.set(getString(R.string.ad_privacy_page_title));
        if (showRestrictedView()) {
            SettingsUtils.addPreferencesFromResource(
                    this, R.xml.privacy_sandbox_preferences_restricted);
        } else {
            SettingsUtils.addPreferencesFromResource(this, R.xml.privacy_sandbox_preferences);
            mTopicsPref = findPreference(TOPICS_PREF);
            mFledgePref = findPreference(FLEDGE_PREF);
        }
        mAdMeasurementPref = findPreference(AD_MEASUREMENT_PREF);

        parseAndRecordReferrer();
    }

    @Override
    public MonotonicObservableSupplier<String> getPageTitle() {
        return mPageTitle;
    }

    @Override
    public void onStart() {
        super.onStart();

        updatePrefDescription();
    }

    private boolean showRestrictedView() {
        return getPrivacySandboxBridge().isPrivacySandboxRestricted();
    }

    private void updatePrefDescription() {
        // LINT.IfChange(RestrictedPrefsSummary)
        if (!showRestrictedView()) {
            assumeNonNull(mTopicsPref);
            mTopicsPref.setSummary(
                    TopicsFragment.isTopicsPrefEnabled(getProfile())
                            ? R.string.ad_privacy_page_topics_link_row_sub_label_enabled
                            : R.string.ad_privacy_page_topics_link_row_sub_label_disabled);

            assumeNonNull(mFledgePref);
            mFledgePref.setSummary(
                    FledgeFragment.isFledgePrefEnabled(getProfile())
                            ? R.string.ad_privacy_page_fledge_link_row_sub_label_enabled
                            : R.string.ad_privacy_page_fledge_link_row_sub_label_disabled);
        }

        mAdMeasurementPref.setSummary(
                AdMeasurementFragment.isAdMeasurementPrefEnabled(getProfile())
                        ? R.string.ad_privacy_page_ad_measurement_link_row_sub_label_enabled
                        : R.string.ad_privacy_page_ad_measurement_link_row_sub_label_disabled);
        // LINT.ThenChange(:DynamicPrefsSummary)
    }

    @Override
    public @SettingsFragment.AnimationType int getAnimationType() {
        return SettingsFragment.AnimationType.PROPERTY;
    }

    public static final ChromeBaseSearchIndexProvider SEARCH_INDEX_DATA_PROVIDER =
            new ChromeBaseSearchIndexProvider(
                    PrivacySandboxSettingsFragment.class.getName(),
                    R.xml.privacy_sandbox_preferences) {
                @Override
                public Bundle getExtras() {
                    Bundle args = new Bundle();
                    args.putInt(PRIVACY_SANDBOX_REFERRER, PrivacySandboxReferrer.PRIVACY_SETTINGS);
                    return args;
                }

                @Override
                public void updateDynamicPreferences(
                        Context context, SettingsIndexData indexData, Profile profile) {
                    PrivacySandboxBridge bridge = new PrivacySandboxBridge(profile);
                    String prefFragment = PrivacySandboxSettingsFragment.class.getName();

                    if (bridge.isPrivacySandboxRestricted()) {
                        indexData.removeEntry(getUniqueId(TOPICS_PREF));
                        indexData.removeEntry(getUniqueId(FLEDGE_PREF));
                    } else {
                        // LINT.IfChange(DynamicPrefsSummary)
                        // The summary for enabled/disable is the same except for the trailing info
                        // (on/off). To reflect that, an index refresh is required and for now that
                        // is not necessary.
                        indexData.updateEntrySummaryForKey(
                                prefFragment,
                                TOPICS_PREF,
                                TopicsFragment.isTopicsPrefEnabled(profile)
                                        ? R.string.ad_privacy_page_topics_link_row_sub_label_enabled
                                        : R.string
                                                .ad_privacy_page_topics_link_row_sub_label_disabled);

                        indexData.updateEntrySummaryForKey(
                                prefFragment,
                                FLEDGE_PREF,
                                FledgeFragment.isFledgePrefEnabled(profile)
                                        ? R.string.ad_privacy_page_fledge_link_row_sub_label_enabled
                                        : R.string
                                                .ad_privacy_page_fledge_link_row_sub_label_disabled);

                        indexData.updateEntrySummaryForKey(
                                prefFragment,
                                AD_MEASUREMENT_PREF,
                                AdMeasurementFragment.isAdMeasurementPrefEnabled(profile)
                                        ? R.string
                                                .ad_privacy_page_ad_measurement_link_row_sub_label_enabled
                                        : R.string
                                                .ad_privacy_page_ad_measurement_link_row_sub_label_disabled);
                        // LINT.ThenChange()
                    }
                }
            };
}
