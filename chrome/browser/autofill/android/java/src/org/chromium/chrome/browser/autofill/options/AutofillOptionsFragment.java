// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.options;

import android.os.Bundle;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;

import androidx.annotation.IntDef;
import androidx.preference.PreferenceFragmentCompat;

import org.chromium.chrome.browser.autofill.R;
import org.chromium.chrome.browser.feedback.FragmentHelpAndFeedbackLauncher;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncher;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.ProfileDependentSetting;
import org.chromium.components.browser_ui.settings.SettingsUtils;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
/**
 * Autofill options fragment, which allows the user to configure autofill.
 */
public class AutofillOptionsFragment extends PreferenceFragmentCompat
        implements FragmentHelpAndFeedbackLauncher, ProfileDependentSetting {
    // Key for the argument with which the AutofillOptions fragment will be launched. The value for
    // this argument is part of the AutofillOptionsReferrer enum containing all entry points.
    public static final String AUTOFILL_OPTIONS_REFERRER = "autofill-options-referrer";
    public static final String PREF_AUTOFILL_THIRD_PARTY_FILLING = "autofill_third_party_filling";

    private Profile mProfile;
    private HelpAndFeedbackLauncher mHelpAndFeedbackLauncher;
    private @AutofillOptionsReferrer int mReferrer;

    // Represents different referrers when navigating to the Autofill Options page.
    //
    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    //
    // Needs to stay in sync with AutofillOptionsReferrer in enums.xml.
    @IntDef({AutofillOptionsReferrer.SETTINGS, AutofillOptionsReferrer.DEEP_LINK_TO_SETTINGS,
            AutofillOptionsReferrer.COUNT})
    @Retention(RetentionPolicy.SOURCE)
    public @interface AutofillOptionsReferrer {
        /**
         * Corresponds to the Settings page.
         */
        int SETTINGS = 0;
        /**
         * Corresponds to an external link opening Chrome.
         */
        int DEEP_LINK_TO_SETTINGS = 1;
        int COUNT = 2;
    }

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
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mReferrer = getReferrerFromInstanceStateOrLaunchBundle(savedInstanceState);
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

    public static Bundle createRequiredArgs(@AutofillOptionsReferrer int referrer) {
        Bundle requiredArgs = new Bundle();
        requiredArgs.putInt(AUTOFILL_OPTIONS_REFERRER, referrer);
        return requiredArgs;
    }

    @AutofillOptionsReferrer
    int getReferrer() {
        return mReferrer;
    }

    @Override
    public void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);
        outState.putInt(AUTOFILL_OPTIONS_REFERRER, mReferrer);
    }

    private @AutofillOptionsReferrer int getReferrerFromInstanceStateOrLaunchBundle(
            Bundle savedInstanceState) {
        if (savedInstanceState != null
                && savedInstanceState.containsKey(AUTOFILL_OPTIONS_REFERRER)) {
            return savedInstanceState.getInt(AUTOFILL_OPTIONS_REFERRER);
        }
        Bundle extras = getArguments();
        assert extras.containsKey(AUTOFILL_OPTIONS_REFERRER)
            : "AutofillOptionsFragment must be launched with a autofill-options-referrer fragment!";
        return extras.getInt(AUTOFILL_OPTIONS_REFERRER);
    }
}
