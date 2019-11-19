// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.autofill;

import android.content.Context;
import android.os.Bundle;
import android.support.v7.preference.Preference;
import android.support.v7.preference.PreferenceFragmentCompat;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeStringConstants;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.preferences.MainPreferences;
import org.chromium.chrome.browser.preferences.PreferenceUtils;

/**
 * Fragment for settings page that allows user to view and edit a single server-provided address.
 */
public class AutofillServerProfilePreferences
        extends PreferenceFragmentCompat implements Preference.OnPreferenceClickListener {
    private String mGUID;

    private static final String PREF_SERVER_PROFILE_DESCRIPTION = "server_profile_description";
    private static final String PREF_SERVER_PROFILE_EDIT_LINK = "server_profile_edit_link";

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        PreferenceUtils.addPreferencesFromResource(this, R.xml.autofill_server_profile_preferences);
        getActivity().setTitle(R.string.autofill_edit_profile);

        // We know which card to display based on the GUID stuffed in
        // our extras by MainPreferences.
        Bundle extras = getArguments();
        if (extras != null) {
            mGUID = extras.getString(MainPreferences.AUTOFILL_GUID);
        }
        assert mGUID != null;
        AutofillProfile profile = PersonalDataManager.getInstance().getProfile(mGUID);
        if (profile == null) {
            getActivity().finish();
            return;
        }

        assert !profile.getIsLocal();

        Preference profileDescription = findPreference(PREF_SERVER_PROFILE_DESCRIPTION);
        profileDescription.setTitle(profile.getFullName());
        profileDescription.setSummary(profile.getStreetAddress());

        findPreference(PREF_SERVER_PROFILE_EDIT_LINK).setOnPreferenceClickListener(this);
    }

    @Override
    public boolean onPreferenceClick(Preference preference) {
        assert preference.getKey().equals(PREF_SERVER_PROFILE_EDIT_LINK);
        Context context = preference.getContext();
        CustomTabActivity.showInfoPage(
                context, ChromeStringConstants.AUTOFILL_MANAGE_WALLET_ADDRESSES_URL);
        return true;
    }
}
