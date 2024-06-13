// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import android.content.Context;
import android.os.Bundle;

import androidx.annotation.VisibleForTesting;
import androidx.fragment.app.Fragment;
import androidx.preference.PreferenceScreen;

import org.chromium.base.Callback;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.settings.ChromeBaseSettingsFragment;

/** Preferences fragment to allow users to manage card benefits linked to their credit cards. */
public class AutofillCardBenefitsFragment extends ChromeBaseSettingsFragment {
    private static Callback<Fragment> sObserverForTest;

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        getActivity().setTitle(R.string.autofill_card_benefits_settings_page_title);

        // Create blank preference screen.
        PreferenceScreen screen = getPreferenceManager().createPreferenceScreen(getStyledContext());
        setPreferenceScreen(screen);
        if (sObserverForTest != null) {
            sObserverForTest.onResult(this);
        }
    }

    private Context getStyledContext() {
        return getPreferenceManager().getContext();
    }

    @VisibleForTesting
    static void setObserverForTest(Callback<Fragment> observerForTest) {
        sObserverForTest = observerForTest;
    }
}
