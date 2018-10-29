// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.autofill;

import android.graphics.PorterDuff;
import android.graphics.drawable.Drawable;
import android.os.Bundle;
import android.preference.Preference;
import android.preference.Preference.OnPreferenceChangeListener;
import android.preference.PreferenceFragment;
import android.support.annotation.VisibleForTesting;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.StrictModeContext;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.preferences.ChromeSwitchPreference;
import org.chromium.chrome.browser.preferences.MainPreferences;
import org.chromium.chrome.browser.preferences.ManagedPreferenceDelegate;
import org.chromium.chrome.browser.preferences.PreferenceUtils;
import org.chromium.chrome.browser.widget.prefeditor.EditorObserverForTest;

/**
 * Autofill profiles fragment, which allows the user to edit autofill profiles.
 */
public class AutofillProfilesFragment
        extends PreferenceFragment implements PersonalDataManager.PersonalDataManagerObserver {
    private static EditorObserverForTest sObserverForTest;
    static final String PREF_NEW_PROFILE = "new_profile";

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        PreferenceUtils.addPreferencesFromResource(
                this, R.xml.autofill_and_payments_preference_fragment_screen);
        getActivity().setTitle(R.string.autofill_addresses_settings_title);
    }

    @Override
    public void onResume() {
        super.onResume();
        // Always rebuild our list of profiles.  Although we could detect if profiles are added or
        // deleted (GUID list changes), the profile summary (name+addr) might be different.  To be
        // safe, we update all.
        rebuildProfileList();
        if (sObserverForTest != null) sObserverForTest.onEditorDismiss();
    }

    private void rebuildProfileList() {
        getPreferenceScreen().removeAll();
        getPreferenceScreen().setOrderingAsAdded(true);

        ChromeSwitchPreference autofillSwitch = new ChromeSwitchPreference(getActivity(), null);
        autofillSwitch.setTitle(R.string.autofill_enable_profiles_toggle_label);
        autofillSwitch.setSummary(R.string.autofill_enable_profiles_toggle_sublabel);
        autofillSwitch.setChecked(PersonalDataManager.isAutofillProfileEnabled());
        autofillSwitch.setOnPreferenceChangeListener(new OnPreferenceChangeListener() {
            @Override
            public boolean onPreferenceChange(Preference preference, Object newValue) {
                PersonalDataManager.setAutofillProfileEnabled((boolean) newValue);
                return true;
            }
        });
        autofillSwitch.setManagedPreferenceDelegate(new ManagedPreferenceDelegate() {
            @Override
            public boolean isPreferenceControlledByPolicy(Preference preference) {
                return PersonalDataManager.isAutofillProfileManaged();
            }

            @Override
            public boolean isPreferenceClickDisabledByPolicy(Preference preference) {
                return PersonalDataManager.isAutofillProfileManaged()
                        && !PersonalDataManager.isAutofillProfileEnabled();
            }
        });
        getPreferenceScreen().addPreference(autofillSwitch);

        for (AutofillProfile profile : PersonalDataManager.getInstance().getProfilesForSettings()) {
            // Add a preference for the profile.
            Preference pref;
            if (profile.getIsLocal()) {
                AutofillProfileEditorPreference localPref =
                        new AutofillProfileEditorPreference(getActivity(), sObserverForTest);
                localPref.setTitle(profile.getFullName());
                localPref.setSummary(profile.getLabel());
                localPref.setKey(localPref.getTitle().toString()); // For testing.
                pref = localPref;
            } else {
                pref = new Preference(getActivity());
                pref.setWidgetLayoutResource(R.layout.autofill_server_data_label);
                pref.setFragment(AutofillServerProfilePreferences.class.getName());
            }
            Bundle args = pref.getExtras();
            args.putString(MainPreferences.AUTOFILL_GUID, profile.getGUID());
            try (StrictModeContext unused = StrictModeContext.allowDiskWrites()) {
                getPreferenceScreen().addPreference(pref);
            }
        }

        // Add 'Add address' button. Tap of it brings up address editor which allows users type in
        // new addresses.
        if (PersonalDataManager.isAutofillProfileEnabled()) {
            AutofillProfileEditorPreference pref =
                    new AutofillProfileEditorPreference(getActivity(), sObserverForTest);
            Drawable plusIcon = ApiCompatibilityUtils.getDrawable(getResources(), R.drawable.plus);
            plusIcon.mutate();
            plusIcon.setColorFilter(
                    ApiCompatibilityUtils.getColor(getResources(), R.color.pref_accent_color),
                    PorterDuff.Mode.SRC_IN);
            pref.setIcon(plusIcon);
            pref.setTitle(R.string.autofill_create_profile);
            pref.setKey(PREF_NEW_PROFILE); // For testing.

            try (StrictModeContext unused = StrictModeContext.allowDiskWrites()) {
                getPreferenceScreen().addPreference(pref);
            }
        }
    }

    @Override
    public void onPersonalDataChanged() {
        rebuildProfileList();
        if (sObserverForTest != null) sObserverForTest.onEditorDismiss();
    }

    @Override
    public void onActivityCreated(Bundle savedInstanceState) {
        super.onActivityCreated(savedInstanceState);
        PersonalDataManager.getInstance().registerDataObserver(this);
    }

    @Override
    public void onDestroyView() {
        PersonalDataManager.getInstance().unregisterDataObserver(this);
        super.onDestroyView();
    }

    @VisibleForTesting
    public static void setObserverForTest(EditorObserverForTest observerForTest) {
        sObserverForTest = observerForTest;
    }
}
