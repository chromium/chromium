// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import android.os.Bundle;
import android.text.SpannableString;
import android.text.TextUtils;
import android.text.style.ForegroundColorSpan;

import androidx.annotation.StringRes;
import androidx.preference.Preference;
import androidx.preference.PreferenceFragmentCompat;

import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.widget.ChromeBulletSpan;

/**
 * Settings fragment for privacy sandbox settings.
 */
public class LearnMoreFragment extends PreferenceFragmentCompat {
    private static final String TOPICS_DESCRIPTION_PREFERENCE = "topics_description";
    /**
     * Initializes all the objects related to the preferences page.
     */
    @Override
    public void onCreatePreferences(Bundle bundle, String s) {
        getActivity().setTitle(R.string.privacy_sandbox_learn_more_title);
        SettingsUtils.addPreferencesFromResource(this, R.xml.learn_more_preference);
        Preference topicDescription = findPreference(TOPICS_DESCRIPTION_PREFERENCE);
        assert topicDescription != null;
        topicDescription.setSelectable(true);
        topicDescription.setSummary(TextUtils.concat(
                formatLearnMoreBullet(R.string.privacy_sandbox_learn_more_description_1), "\n\n",
                formatLearnMoreBullet(R.string.privacy_sandbox_learn_more_description_2), "\n\n",
                formatLearnMoreBullet(R.string.privacy_sandbox_learn_more_description_3)));
    }

    private SpannableString formatLearnMoreBullet(@StringRes int stringId) {
        String string = getContext().getString(stringId);
        SpannableString spannableString = SpanApplier.applySpans(string,
                new SpanApplier.SpanInfo("<b>", "</b>",
                        new ForegroundColorSpan(
                                SemanticColorUtils.getDefaultTextColor(getContext()))));
        spannableString.setSpan(new ChromeBulletSpan(getContext()), 0, spannableString.length(), 0);
        return spannableString;
    }
}
