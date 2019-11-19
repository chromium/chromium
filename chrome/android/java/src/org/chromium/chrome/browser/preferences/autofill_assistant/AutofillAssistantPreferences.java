// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.autofill_assistant;

import android.content.Context;
import android.os.Bundle;
import android.support.v7.preference.PreferenceFragmentCompat;
import android.support.v7.preference.PreferenceScreen;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.preferences.ChromeSwitchPreference;

/** The "Autofill Assistant" preferences screen in Settings. */
public class AutofillAssistantPreferences extends PreferenceFragmentCompat {
    /** Autofill Assistant switch preference key name. */
    public static final String PREF_AUTOFILL_ASSISTANT_SWITCH = "autofill_assistant_switch";

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        getActivity().setTitle(R.string.prefs_autofill_assistant_title);

        PreferenceScreen screen = getPreferenceManager().createPreferenceScreen(getStyledContext());
        setPreferenceScreen(screen);
        createAutofillAssistantSwitch();
    }

    private void createAutofillAssistantSwitch() {
        ChromeSwitchPreference autofillAssistantSwitch =
                new ChromeSwitchPreference(getStyledContext(), null);
        autofillAssistantSwitch.setKey(PREF_AUTOFILL_ASSISTANT_SWITCH);
        autofillAssistantSwitch.setTitle(R.string.prefs_autofill_assistant_switch);
        autofillAssistantSwitch.setSummaryOn(R.string.text_on);
        autofillAssistantSwitch.setSummaryOff(R.string.text_off);
        autofillAssistantSwitch.setOnPreferenceChangeListener((preference, newValue) -> {
            ContextUtils.getAppSharedPreferences()
                    .edit()
                    .putBoolean(PREF_AUTOFILL_ASSISTANT_SWITCH, (boolean) newValue)
                    .apply();
            return true;
        });
        getPreferenceScreen().addPreference(autofillAssistantSwitch);

        // Note: setting the switch state before the preference is added to the screen results in
        // some odd behavior where the switch state doesn't always match the internal enabled state
        // (e.g. the switch will say "On" when it is really turned off), so .setChecked() should be
        // called after .addPreference()
        autofillAssistantSwitch.setChecked(ContextUtils.getAppSharedPreferences().getBoolean(
                PREF_AUTOFILL_ASSISTANT_SWITCH, true));
    }

    private Context getStyledContext() {
        return getPreferenceManager().getContext();
    }
}
