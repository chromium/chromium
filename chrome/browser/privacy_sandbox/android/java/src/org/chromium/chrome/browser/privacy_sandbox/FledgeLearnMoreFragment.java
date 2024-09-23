// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import android.os.Bundle;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.View;

import androidx.annotation.NonNull;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.browser_ui.settings.TextMessagePreference;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;

/** Settings fragment for privacy sandbox settings. */
public class FledgeLearnMoreFragment extends PrivacySandboxSettingsBaseFragment {
    private static final String FLEDGE_LEARN_MORE_BULLET_3_PREFERENCE =
            "fledge_learn_more_bullet_3";

    private TextMessagePreference mFledgeLearnMoreBullet3Preference;
    private final ObservableSupplierImpl<String> mPageTitle = new ObservableSupplierImpl<>();

    /** Initializes all the objects related to the preferences page. */
    @Override
    public void onCreatePreferences(Bundle bundle, String s) {
        mPageTitle.set(getString(R.string.settings_fledge_page_title));
        SettingsUtils.addPreferencesFromResource(this, R.xml.fledge_learn_more_preference);
        mFledgeLearnMoreBullet3Preference = findPreference(FLEDGE_LEARN_MORE_BULLET_3_PREFERENCE);
        mFledgeLearnMoreBullet3Preference.setSummary(
                SpanApplier.applySpans(
                        getResources().getString(R.string.settings_fledge_page_learn_more_bullet_3),
                        new SpanApplier.SpanInfo(
                                "<link>",
                                "</link>",
                                new NoUnderlineClickableSpan(
                                        getContext(), this::onLearnMoreClicked))));
        // Enable the options menu to be able to clear it.
        setHasOptionsMenu(true);
    }

    @Override
    public ObservableSupplier<String> getPageTitle() {
        return mPageTitle;
    }

    private void onLearnMoreClicked(View view) {
        openUrlInCct(PrivacySandboxSettingsFragment.HELP_CENTER_URL);
    }

    @Override
    public void onCreateOptionsMenu(@NonNull Menu menu, @NonNull MenuInflater inflater) {
        super.onCreateOptionsMenu(menu, inflater);
        menu.clear();
    }
}
