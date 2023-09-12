// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox.v4;

import android.os.Bundle;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.privacy_sandbox.PrivacySandboxBridge;
import org.chromium.chrome.browser.privacy_sandbox.PrivacySandboxSettingsBaseFragment;
import org.chromium.chrome.browser.privacy_sandbox.R;
import org.chromium.components.browser_ui.settings.ChromeBasePreference;
import org.chromium.components.browser_ui.settings.SettingsUtils;

/**
 * Settings fragment for privacy sandbox settings.
 */
public class PrivacySandboxSettingsFragmentV4 extends PrivacySandboxSettingsBaseFragment {
    public static final String TOPICS_PREF = "topics";
    public static final String FLEDGE_PREF = "fledge";
    public static final String AD_MEASUREMENT_PREF = "ad_measurement";
    public static final String HELP_CENTER_URL = "https://support.google.com/chrome/?p=ad_privacy";

    private ChromeBasePreference mTopicsPref;
    private ChromeBasePreference mFledgePref;
    private ChromeBasePreference mAdMeasurementPref;

    @Override
    public void onCreatePreferences(@Nullable Bundle bundle, @Nullable String s) {
        super.onCreatePreferences(bundle, s);
        assert ChromeFeatureList.isEnabled(ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS_4);

        // This view should not be shown when PS is restricted, unless the
        // isRestrictedNoticeEnabled flag is enabled.
        assert !PrivacySandboxBridge.isPrivacySandboxRestricted()
                || PrivacySandboxBridge.isRestrictedNoticeEnabled();

        // Add all preferences and set the title
        getActivity().setTitle(R.string.ad_privacy_page_title);
        if (showRestrictedView()) {
            SettingsUtils.addPreferencesFromResource(
                    this, R.xml.privacy_sandbox_preferences_restricted_v4);
        } else {
            SettingsUtils.addPreferencesFromResource(this, R.xml.privacy_sandbox_preferences_v4);
            mTopicsPref = findPreference(TOPICS_PREF);
            mFledgePref = findPreference(FLEDGE_PREF);
        }
        mAdMeasurementPref = findPreference(AD_MEASUREMENT_PREF);

        parseAndRecordReferrer();
    }

    @Override
    public void onResume() {
        super.onResume();

        updatePrefDescription();
    }

    private boolean showRestrictedView() {
        return PrivacySandboxBridge.isPrivacySandboxRestricted();
    }

    private void updatePrefDescription() {
        if (!showRestrictedView()) {
            mTopicsPref.setSummary(TopicsFragmentV4.isTopicsPrefEnabled(getProfile())
                            ? R.string.ad_privacy_page_topics_link_row_sub_label_enabled
                            : R.string.ad_privacy_page_topics_link_row_sub_label_disabled);

            mFledgePref.setSummary(FledgeFragmentV4.isFledgePrefEnabled(getProfile())
                            ? R.string.ad_privacy_page_fledge_link_row_sub_label_enabled
                            : R.string.ad_privacy_page_fledge_link_row_sub_label_disabled);
        }

        mAdMeasurementPref.setSummary(
                AdMeasurementFragmentV4.isAdMeasurementPrefEnabled(getProfile())
                        ? R.string.ad_privacy_page_ad_measurement_link_row_sub_label_enabled
                        : R.string.ad_privacy_page_ad_measurement_link_row_sub_label_disabled);
    }
}
