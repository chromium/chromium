// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.options;

import android.os.Bundle;

import androidx.preference.PreferenceFragmentCompat;

import org.chromium.chrome.browser.autofill.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.ProfileDependentSetting;

/**
 * Autofill options fragment, which allows the user to configure autofill.
 */
public class AutofillOptionsFragment
        extends PreferenceFragmentCompat implements ProfileDependentSetting {
    private Profile mProfile;

    /**
     * This default constructor is required to instantiate the fragment.
     */
    public AutofillOptionsFragment() {}

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        getActivity().setTitle(R.string.autofill_options_title);
    }

    /**
     * As {@link ProfileDependentSetting}, the settings activity calls {@link setProfile} when the
     * fragment is attached. The getter allows to use the injected profile in the coordinator.
     *
     * @return A {@link Profile}.
     */
    Profile getProfile() {
        return mProfile;
    }

    @Override
    public void setProfile(Profile profile) {
        mProfile = profile;
    }
}
