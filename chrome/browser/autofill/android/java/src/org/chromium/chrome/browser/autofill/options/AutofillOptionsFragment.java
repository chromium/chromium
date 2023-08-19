// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.options;

import android.os.Bundle;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;

import androidx.preference.PreferenceFragmentCompat;

import org.chromium.chrome.browser.autofill.R;
import org.chromium.chrome.browser.feedback.FragmentHelpAndFeedbackLauncher;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncher;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.ProfileDependentSetting;
import org.chromium.components.browser_ui.settings.SettingsUtils;
/**
 * Autofill options fragment, which allows the user to configure autofill.
 */
public class AutofillOptionsFragment extends PreferenceFragmentCompat
        implements FragmentHelpAndFeedbackLauncher, ProfileDependentSetting {
    public static final String PREF_AUTOFILL_THIRD_PARTY_FILLING = "autofill_third_party_filling";

    private Profile mProfile;
    private HelpAndFeedbackLauncher mHelpAndFeedbackLauncher;

    /**
     * This default constructor is required to instantiate the fragment.
     */
    public AutofillOptionsFragment() {}

    RadioButtonGroupThirdPartyPreference getThirdPartyFillingOption() {
        RadioButtonGroupThirdPartyPreference thirdPartyFillingSwitch =
                findPreference(PREF_AUTOFILL_THIRD_PARTY_FILLING);
        assert thirdPartyFillingSwitch != null;
        return thirdPartyFillingSwitch;
    }

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        getActivity().setTitle(R.string.autofill_options_title);
        setHasOptionsMenu(true);
        SettingsUtils.addPreferencesFromResource(this, R.xml.autofill_options_preferences);
    }

    @Override
    public void onCreateOptionsMenu(Menu menu, MenuInflater inflater) {
        menu.clear();
        MenuItem help =
                menu.add(Menu.NONE, R.id.menu_id_targeted_help, Menu.NONE, R.string.menu_help);
        help.setIcon(R.drawable.ic_help_and_feedback);
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        if (item.getItemId() == R.id.menu_id_targeted_help) {
            mHelpAndFeedbackLauncher.show(
                    getActivity(), getActivity().getString(R.string.help_context_autofill), null);
            return true;
        }
        return super.onOptionsItemSelected(item);
    }

    @Override
    public void setHelpAndFeedbackLauncher(HelpAndFeedbackLauncher helpAndFeedbackLauncher) {
        mHelpAndFeedbackLauncher = helpAndFeedbackLauncher;
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
