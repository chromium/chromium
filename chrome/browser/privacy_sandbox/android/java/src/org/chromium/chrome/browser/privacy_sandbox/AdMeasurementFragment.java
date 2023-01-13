// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import android.os.Bundle;
import android.view.View;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.browser_ui.settings.ChromeBasePreference;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;

/**
 * Settings fragment for privacy sandbox settings.
 */
public class AdMeasurementFragment extends PrivacySandboxSettingsBaseFragment {
    private static final String AD_MEASUREMENT_DESCRIPTION = "ad_measurement_description";
    private Runnable mOpenHistoryRunnable;

    /**
     * Initializes all the objects related to the preferences page.
     */
    @Override
    public void onCreatePreferences(Bundle bundle, String s) {
        assert (!ChromeFeatureList.isEnabled(ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS_4));

        super.onCreatePreferences(bundle, s);
        getActivity().setTitle(R.string.privacy_sandbox_ad_measurement_title);
        SettingsUtils.addPreferencesFromResource(this, R.xml.ad_measurement_preference);

        ChromeBasePreference descriptionPreference = findPreference(AD_MEASUREMENT_DESCRIPTION);
        int description = PrivacySandboxBridge.isPrivacySandboxEnabled()
                ? R.string.privacy_sandbox_ad_measurement_description_trials_on
                : R.string.privacy_sandbox_ad_measurement_description_trials_off;
        descriptionPreference.setSummary(
                SpanApplier.applySpans(getResources().getString(description),
                        new SpanApplier.SpanInfo("<link>", "</link>",
                                new NoUnderlineClickableSpan(getContext(), this::showHistory))));
    }

    /**
     * Set the a helper to open history from settings.
     */
    public void setSetHistoryHelper(Runnable openHistoryRunnable) {
        mOpenHistoryRunnable = openHistoryRunnable;
    }

    private void showHistory(View view) {
        mOpenHistoryRunnable.run();
    }
}
