// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager.settings;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.widget.TextView;

import androidx.preference.PreferenceViewHolder;

import org.chromium.chrome.browser.access_loss.PasswordAccessLossWarningType;
import org.chromium.chrome.browser.password_manager.PasswordManagerHelper;
import org.chromium.chrome.browser.password_manager.R;
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
        setUpPasswordAccessLossWarning(holder);
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

    public void setUpPasswordAccessLossWarning(PreferenceViewHolder holder) {
        assert mProfile != null : "Profile is not set!";
        PrefService prefService = UserPrefs.get(mProfile);
        if (PasswordManagerHelper.getAccessLossWarningType(prefService)
                == PasswordAccessLossWarningType.NONE) return;

        TextView summaryView = (TextView) holder.findViewById(android.R.id.summary);
        summaryView.setText(R.string.access_loss_pref_desc);
        // ChromeBasePreference sets summary text view to be not visible by default if it's empty.
        // So explicitly setting it to visible here.
        summaryView.setVisibility(View.VISIBLE);
        setWidgetLayoutResource(R.layout.passwords_preference_error_widget);
    }
}
