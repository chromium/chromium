// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager.settings;

import android.content.Context;
import android.util.AttributeSet;

import androidx.preference.PreferenceViewHolder;

import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.ProfileDependentSetting;
import org.chromium.components.browser_ui.settings.ChromeBasePreference;
import org.chromium.components.browser_ui.widget.text.TextViewWithCompoundDrawables;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;

/**
 * Password manager preference to be displayed in the main settings screen. A custom implementation
 * is used so that the password manager entry point displays custom strings when saving passwords is
 * enabled/disabled by policy.
 */
public class PasswordsPreference extends ChromeBasePreference implements ProfileDependentSetting {
    private Profile mProfile;

    /** Constructor for inflating from XML. */
    public PasswordsPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    public void setProfile(Profile profile) {
        mProfile = profile;
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);
        // This preference is the only one of its type in the preferences group so it will not
        // be recycled.
        PrefService prefService = UserPrefs.get(mProfile);
        if (!prefService.isManagedPreference(Pref.CREDENTIALS_ENABLE_SERVICE)) {
            return;
        }

        TextViewWithCompoundDrawables managedDisclaimerView =
                holder.itemView.findViewById(
                        org.chromium.components.browser_ui.settings.R.id.managed_disclaimer_text);
        assert managedDisclaimerView != null;
        boolean offerToSavePasswords =
                UserPrefs.get(mProfile).getBoolean(Pref.CREDENTIALS_ENABLE_SERVICE);
        managedDisclaimerView.setText(
                offerToSavePasswords
                        ? org.chromium.chrome.browser.password_manager.R.string
                                .password_saving_on_by_administrator
                        : org.chromium.chrome.browser.password_manager.R.string
                                .password_saving_off_by_administrator);
    }
}
