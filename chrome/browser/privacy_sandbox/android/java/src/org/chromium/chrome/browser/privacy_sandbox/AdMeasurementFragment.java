// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import android.os.Bundle;
import android.view.View;

import androidx.preference.PreferenceFragmentCompat;

import org.chromium.components.browser_ui.settings.ChromeBasePreference;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;

/**
 * Settings fragment for privacy sandbox settings.
 */
public class AdMeasurementFragment extends PreferenceFragmentCompat {
    public static final String AD_MEASUREMENT_DESCRIPTION = "ad_measurement_description";

    /**
     * Initializes all the objects related to the preferences page.
     */
    @Override
    public void onCreatePreferences(Bundle bundle, String s) {
        getActivity().setTitle(R.string.privacy_sandbox_ad_measurement_title);
        SettingsUtils.addPreferencesFromResource(this, R.xml.ad_measurement_preference);

        ChromeBasePreference descriptionPreference = findPreference(AD_MEASUREMENT_DESCRIPTION);
        descriptionPreference.setSummary(SpanApplier.applySpans(
                getResources().getString(R.string.privacy_sandbox_ad_measurement_description),
                new SpanApplier.SpanInfo("<link>", "</link>",
                        new NoUnderlineClickableSpan(getContext(), this::showHistory))));
    }

    private void showHistory(View view) {
        // TODO(crbug.com/1286276): Show history page
    }
}
