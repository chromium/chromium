// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safe_browsing.settings;

import android.os.Bundle;
import android.view.View;

import androidx.annotation.VisibleForTesting;

import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;

/** Fragment containing enhanced protection settings. */
public class EnhancedProtectionSettingsFragment extends SafeBrowsingSettingsFragmentBase {
    @VisibleForTesting static final String PREF_LEARN_MORE = "learn_more";
    private static final String SAFE_BROWSING_IN_CHROME_URL =
            "https://support.google.com/chrome?p=safebrowsing_in_chrome";

    @Override
    protected int getPreferenceResource() {
        return R.xml.enhanced_protection_preferences;
    }

    @Override
    protected void onCreatePreferencesInternal(Bundle bundle, String s) {
        findPreference(PREF_LEARN_MORE)
                .setSummary(
                        SpanApplier.applySpans(
                                getResources()
                                        .getString(
                                                R.string
                                                        .safe_browsing_enhanced_protection_learn_more_label),
                                new SpanApplier.SpanInfo(
                                        "<link>",
                                        "</link>",
                                        new NoUnderlineClickableSpan(
                                                getContext(), this::onLearnMoreClicked))));
    }

    private void onLearnMoreClicked(View view) {
        openUrlInCct(SAFE_BROWSING_IN_CHROME_URL);
    }
}
