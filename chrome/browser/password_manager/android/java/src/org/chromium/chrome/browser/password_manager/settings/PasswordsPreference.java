// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager.settings;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.flags.ChromeFeatureList.LOGIN_DB_DEPRECATION_ANDROID;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.widget.TextView;

import androidx.preference.PreferenceViewHolder;

import org.chromium.base.DeviceInfo;
import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.access_loss.PasswordAccessLossWarningType;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.password_manager.LoginDbDeprecationUtilBridge;
import org.chromium.chrome.browser.password_manager.PasswordAccessLossDialogHelper;
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

        if (!ChromeFeatureList.isEnabled(LOGIN_DB_DEPRECATION_ANDROID)) {
            setUpAccessLossWarning(holder, prefService);
        }

        if (prefService.isManagedPreference(Pref.CREDENTIALS_ENABLE_SERVICE)) {
            setUpManagedDisclaimerView(holder, prefService);
        }

        if (ChromeFeatureList.isEnabled(LOGIN_DB_DEPRECATION_ANDROID)) {
            // This warning should override the managed text if any is present.
            setUpPostDeprecationWarning(holder, prefService);
        }
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

    private void setUpPostDeprecationWarning(PreferenceViewHolder holder, PrefService prefService) {
        assert mProfile != null : "Profile is not set!";

        boolean isPasswordManagerAvailable =
                PasswordManagerUtilBridge.isPasswordManagerAvailable(prefService);
        boolean hasPasswordsInCsv = LoginDbDeprecationUtilBridge.hasPasswordsInCsv(mProfile);

        // If there are no unmigrated passwords left in Chrome and the password manager is available
        // no subtitle is needed.
        if (isPasswordManagerAvailable && !hasPasswordsInCsv) {
            return;
        }

        // If the password manager is not available, but the auto-export hasn't finished yet
        // don't show any subtitle either, because clicking the button will have no effect anyway
        // and the version of the subtitle cannot be accurately determined.
        if (!isPasswordManagerAvailable
                && !prefService.getBoolean(Pref.UPM_UNMIGRATED_PASSWORDS_EXPORTED)) {
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

    private void setUpAccessLossWarning(PreferenceViewHolder holder, PrefService prefService) {
        assert mProfile != null : "Profile is not set!";
        @PasswordAccessLossWarningType
        int warningType = PasswordAccessLossDialogHelper.getAccessLossWarningType(prefService);

        // If the device doesn't support Google Play Services and the user exports the local
        // passwords from Chrome, there is no need to show the warning anymore (warning type is
        // NONE), but there is a dialog that will show when the user tries to open the password
        // manager item in settings and the summary that shows up for the passwords preference.
        // If there is no need to show any warning or summary, this method can early return.
        if (warningType == PasswordAccessLossWarningType.NONE) {
            return;
        }

        boolean shouldShowNoticeDialogWithoutPwds =
                (warningType == PasswordAccessLossWarningType.NO_GMS_CORE
                        && prefService.getBoolean(Pref.EMPTY_PROFILE_STORE_LOGIN_DATABASE));
        TextView summaryView = (TextView) holder.findViewById(android.R.id.summary);
        summaryView.setText(getSummaryViewString(shouldShowNoticeDialogWithoutPwds, warningType));
        // ChromeBasePreference sets summary text view to be not visible by default if it's empty.
        // So explicitly setting it to visible here.
        summaryView.setVisibility(View.VISIBLE);
        // TODO: crbug.com/372868129 - Make the icon reliably show up, it's flaky at the moment.
        if (shouldShowNoticeDialogWithoutPwds) {
            setWidgetLayoutResource(R.layout.passwords_preference_info_widget);
            return;
        }
        setWidgetLayoutResource(R.layout.passwords_preference_error_widget);
    }

    private int getSummaryViewString(
            boolean shouldShowNoticeDialogWithoutPwds,
            @PasswordAccessLossWarningType int warningType) {
        if (shouldShowNoticeDialogWithoutPwds) {
            return R.string.access_loss_pref_desc_no_pwds;
        }

        switch (warningType) {
            case PasswordAccessLossWarningType.NO_GMS_CORE:
                return R.string.access_loss_pref_desc_no_gms_core;
            case PasswordAccessLossWarningType.NO_UPM:
                return R.string.access_loss_pref_desc_no_upm;
            case PasswordAccessLossWarningType.ONLY_ACCOUNT_UPM:
                return R.string.access_loss_pref_desc_only_account_upm;
            case PasswordAccessLossWarningType.NEW_GMS_CORE_MIGRATION_FAILED:
                return R.string.access_loss_pref_desc_migration_failed;
        }
        assert false : "Unhandled warning type: " + warningType;
        return 0;
    }
}
