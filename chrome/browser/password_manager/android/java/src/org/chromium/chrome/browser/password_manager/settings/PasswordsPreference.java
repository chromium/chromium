// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager.settings;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.widget.TextView;

import androidx.preference.PreferenceViewHolder;

import org.chromium.base.DeviceInfo;
import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.password_manager.LoginDbDeprecationUtilBridge;
import org.chromium.chrome.browser.password_manager.PasswordManagerUtilBridge;
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
@NullMarked
public class PasswordsPreference extends ChromeBasePreference implements ProfileDependentSetting {
    private static @Nullable PrefService sPrefServiceForTesting;
    private @Nullable Profile mProfile;

    /** Constructor for inflating from XML. */
    public PasswordsPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    /** For testing only. */
    public static void setPrefServiceForTesting(PrefService prefService) {
        sPrefServiceForTesting = prefService;
        ResettersForTesting.register(() -> sPrefServiceForTesting = null);
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
        assumeNonNull(mProfile);
        PrefService prefService =
                sPrefServiceForTesting != null ? sPrefServiceForTesting : UserPrefs.get(mProfile);

        if (prefService.isManagedPreference(Pref.CREDENTIALS_ENABLE_SERVICE)) {
            setUpManagedDisclaimerView(holder, prefService);
        }

        // This warning should override the managed text if any is present.
        setUpPostDeprecationWarning(holder);
    }

    private void setUpManagedDisclaimerView(PreferenceViewHolder holder, PrefService prefService) {
        TextViewWithCompoundDrawables managedDisclaimerView =
                holder.itemView.findViewById(
                        org.chromium.components.browser_ui.settings.R.id.managed_disclaimer_text);

        assert managedDisclaimerView != null;
        boolean offerToSavePasswords = prefService.getBoolean(Pref.CREDENTIALS_ENABLE_SERVICE);
        managedDisclaimerView.setText(
                offerToSavePasswords
                        ? org.chromium.chrome.browser.password_manager.R.string
                                .password_saving_on_by_administrator
                        : org.chromium.chrome.browser.password_manager.R.string
                                .password_saving_off_by_administrator);
    }

    private void setUpPostDeprecationWarning(PreferenceViewHolder holder) {
        assert mProfile != null : "Profile is not set!";

        boolean isPasswordManagerAvailable = PasswordManagerUtilBridge.isPasswordManagerAvailable();
        boolean hasPasswordsInCsv = LoginDbDeprecationUtilBridge.hasPasswordsInCsv(mProfile);

        // If there are no unmigrated passwords left in Chrome and the password manager is available
        // no subtitle is needed.
        if (isPasswordManagerAvailable && !hasPasswordsInCsv) {
            return;
        }

        // If either the password manager is not available or it is available but there are
        // unmigrated passwords left in Chrome, show a subtitle notifying the user of that.
        // Automotive doesn't support the export flow, so only the "stopped working"
        // subtitle is relevant there.
        TextView summaryView = (TextView) holder.findViewById(android.R.id.summary);
        if (!DeviceInfo.isAutomotive() && isPasswordManagerAvailable && hasPasswordsInCsv) {
            summaryView.setText(R.string.some_passwords_are_not_accessible_subtitle);
        } else if (!isPasswordManagerAvailable) {
            summaryView.setText(R.string.gpm_stopped_working_subtitle);
        } else {
            // No error subtitle also means no error icon, so return before the icon would be set.
            return;
        }

        // ChromeBasePreference sets summary text view to be not visible by default if it's empty.
        // So explicitly setting it to visible here.
        summaryView.setVisibility(View.VISIBLE);
        setWidgetLayoutResource(R.layout.passwords_preference_error_widget);
    }
}
