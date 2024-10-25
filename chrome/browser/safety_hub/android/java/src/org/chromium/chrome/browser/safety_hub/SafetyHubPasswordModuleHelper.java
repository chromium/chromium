// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import android.content.Context;

import androidx.annotation.IntDef;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.safety_hub.SafetyHubModuleProperties.ModuleState;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

// Helper class for the Safety Hub Password Module. It allows its clients to easily update the
// preference with characteristics of the module, such as title, summary and buttons.
public final class SafetyHubPasswordModuleHelper {
    /**
     * This should match the default value for {@link
     * org.chromium.chrome.browser.preferences.Pref.BREACHED_CREDENTIALS_COUNT}.
     */
    private static final int INVALID_BREACHED_CREDENTIALS_COUNT = -1;

    // Represents the type of password module.
    @IntDef({
        ModuleType.SIGNED_OUT,
        ModuleType.UNAVAILABLE_PASSWORDS,
        ModuleType.NO_SAVED_PASSWORDS,
        ModuleType.HAS_COMPROMISED_PASSWORDS,
        ModuleType.NO_COMPROMISED_PASSWORDS,
        ModuleType.HAS_WEAK_PASSWORDS,
        ModuleType.HAS_REUSED_PASSWORDS,
        ModuleType.UNAVAILABLE_COMPROMISED_NO_WEAK_REUSED_PASSWORDS
    })
    @Retention(RetentionPolicy.SOURCE)
    private @interface ModuleType {
        int SIGNED_OUT = 0;
        int UNAVAILABLE_PASSWORDS = 1;
        int NO_SAVED_PASSWORDS = 2;
        int HAS_COMPROMISED_PASSWORDS = 3;
        int NO_COMPROMISED_PASSWORDS = 4;
        int HAS_WEAK_PASSWORDS = 5;
        int HAS_REUSED_PASSWORDS = 6;
        int UNAVAILABLE_COMPROMISED_NO_WEAK_REUSED_PASSWORDS = 7;
    };

    // Returns the password module type according to the `model` properties.
    private static @ModuleType int getModuleType(PropertyModel model) {
        int compromisedPasswordsCount =
                model.get(SafetyHubModuleProperties.COMPROMISED_PASSWORDS_COUNT);
        int weakPasswordsCount = model.get(SafetyHubModuleProperties.WEAK_PASSWORDS_COUNT);
        int reusedPasswordsCount = model.get(SafetyHubModuleProperties.REUSED_PASSWORDS_COUNT);
        int totalPasswordsCount = model.get(SafetyHubModuleProperties.TOTAL_PASSWORDS_COUNT);
        boolean isSignedOut = !model.get(SafetyHubModuleProperties.IS_SIGNED_IN);

        boolean isWeakAndReusedFeatureEnabled =
                ChromeFeatureList.sSafetyHubWeakAndReusedPasswords.isEnabled();

        if (isSignedOut) {
            assert compromisedPasswordsCount == INVALID_BREACHED_CREDENTIALS_COUNT;
            return ModuleType.SIGNED_OUT;
        }
        if (compromisedPasswordsCount == INVALID_BREACHED_CREDENTIALS_COUNT) {
            if (isWeakAndReusedFeatureEnabled
                    && weakPasswordsCount == 0
                    && reusedPasswordsCount == 0) {
                return ModuleType.UNAVAILABLE_COMPROMISED_NO_WEAK_REUSED_PASSWORDS;
            }

            return ModuleType.UNAVAILABLE_PASSWORDS;
        }
        if (totalPasswordsCount == 0) {
            return ModuleType.NO_SAVED_PASSWORDS;
        }
        if (compromisedPasswordsCount > 0) {
            return ModuleType.HAS_COMPROMISED_PASSWORDS;
        }
        if (isWeakAndReusedFeatureEnabled) {
            // Reused passwords take priority over the weak passwords count.
            if (reusedPasswordsCount > 0) {
                return ModuleType.HAS_REUSED_PASSWORDS;
            }
            if (weakPasswordsCount > 0) {
                return ModuleType.HAS_WEAK_PASSWORDS;
            }
        }

        // If both reused passwords and weak passwords counts are invalid, ignore them in favour
        // of showing the compromised passwords count.
        return ModuleType.NO_COMPROMISED_PASSWORDS;
    }

    // Updates `preference` for the password module of type {@link ModuleType.SIGNED_OUT}.
    private static void updatePreferenceForSignedOut(
            SafetyHubExpandablePreference preference, PropertyModel model) {
        Context context = preference.getContext();
        preference.setTitle(
                context.getString(R.string.safety_hub_password_check_unavailable_title));
        preference.setSummary(
                context.getString(R.string.safety_hub_password_check_signed_out_summary));
        preference.setPrimaryButtonText(null);
        preference.setPrimaryButtonClickListener(null);
        preference.setSecondaryButtonText(context.getString(R.string.sign_in_to_chrome));
        preference.setSecondaryButtonClickListener(
                model.get(SafetyHubModuleProperties.SAFE_STATE_BUTTON_LISTENER));
    }

    // Updates `preference` for the password module of type {@link
    // ModuleType.UNAVAILABLE_PASSWORDS}.
    private static void updatePreferenceForUnavailablePasswords(
            SafetyHubExpandablePreference preference, PropertyModel model) {
        Context context = preference.getContext();
        preference.setTitle(
                context.getString(R.string.safety_hub_password_check_unavailable_title));
        preference.setSummary(context.getString(R.string.safety_hub_unavailable_summary));
        preference.setPrimaryButtonText(null);
        preference.setPrimaryButtonClickListener(null);
        preference.setSecondaryButtonText(
                context.getString(R.string.safety_hub_passwords_navigation_button));
        preference.setSecondaryButtonClickListener(
                model.get(SafetyHubModuleProperties.SAFE_STATE_BUTTON_LISTENER));
    }

    // Updates `preference` for the password module of type {@link ModuleType.NO_SAVED_PASSWORDS}.
    private static void updatePreferenceForNoSavedPasswords(
            SafetyHubExpandablePreference preference, PropertyModel model) {
        Context context = preference.getContext();
        preference.setTitle(context.getString(R.string.safety_hub_no_passwords_title));
        preference.setSummary(context.getString(R.string.safety_hub_no_passwords_summary));
        preference.setPrimaryButtonText(null);
        preference.setPrimaryButtonClickListener(null);
        preference.setSecondaryButtonText(
                context.getString(R.string.safety_hub_passwords_navigation_button));
        preference.setSecondaryButtonClickListener(
                model.get(SafetyHubModuleProperties.SAFE_STATE_BUTTON_LISTENER));
    }

    // Updates `preference` for the password module of type {@link
    // ModuleType.HAS_COMPROMISED_PASSWORDS}.
    private static void updatePreferenceForHasCompromisedPasswords(
            SafetyHubExpandablePreference preference, PropertyModel model) {
        Context context = preference.getContext();
        int compromisedPasswordsCount =
                model.get(SafetyHubModuleProperties.COMPROMISED_PASSWORDS_COUNT);
        preference.setTitle(
                context.getResources()
                        .getQuantityString(
                                R.plurals.safety_check_passwords_compromised_exist,
                                compromisedPasswordsCount,
                                compromisedPasswordsCount));
        preference.setSummary(
                context.getResources()
                        .getQuantityString(
                                R.plurals.safety_hub_compromised_passwords_summary,
                                compromisedPasswordsCount,
                                compromisedPasswordsCount));
        preference.setPrimaryButtonText(
                context.getString(R.string.safety_hub_passwords_navigation_button));
        preference.setPrimaryButtonClickListener(
                model.get(SafetyHubModuleProperties.SAFE_STATE_BUTTON_LISTENER));
        preference.setSecondaryButtonText(null);
        preference.setSecondaryButtonClickListener(null);
    }

    // Updates `preference` for the password module of type {@link
    // ModuleType.NO_COMPROMISED_PASSWORDS}.
    private static void updatePreferenceForNoCompromisedPasswords(
            SafetyHubExpandablePreference preference, PropertyModel model) {
        Context context = preference.getContext();
        String account = model.get(SafetyHubModuleProperties.ACCOUNT_EMAIL);

        preference.setTitle(context.getString(R.string.safety_hub_no_compromised_passwords_title));

        if (account != null) {
            preference.setSummary(
                    context.getString(R.string.safety_hub_password_check_time_recently, account));
        } else {
            preference.setSummary(context.getString(R.string.safety_hub_checked_recently));
        }
        preference.setPrimaryButtonText(null);
        preference.setPrimaryButtonClickListener(null);
        preference.setSecondaryButtonText(
                context.getString(R.string.safety_hub_passwords_navigation_button));
        preference.setSecondaryButtonClickListener(
                model.get(SafetyHubModuleProperties.SAFE_STATE_BUTTON_LISTENER));
    }

    // Updates `preference` for the password module of type {@link ModuleType.HAS_WEAK_PASSWORDS}.
    private static void updatePreferenceForHasWeakPasswords(
            SafetyHubExpandablePreference preference, PropertyModel model) {
        Context context = preference.getContext();
        int weakPasswordsCount = model.get(SafetyHubModuleProperties.WEAK_PASSWORDS_COUNT);
        preference.setTitle(context.getString(R.string.safety_hub_reused_weak_passwords_title));
        preference.setSummary(
                context.getResources()
                        .getQuantityString(
                                R.plurals.safety_hub_weak_passwords_summary,
                                weakPasswordsCount,
                                weakPasswordsCount));
        preference.setPrimaryButtonText(
                context.getString(R.string.safety_hub_passwords_navigation_button));
        preference.setPrimaryButtonClickListener(
                model.get(SafetyHubModuleProperties.SAFE_STATE_BUTTON_LISTENER));
        preference.setSecondaryButtonText(null);
        preference.setSecondaryButtonClickListener(null);
    }

    // Updates `preference` for the password module of type {@link ModuleType.HAS_REUSED_PASSWORDS}.
    private static void updatePreferenceForHasReusedPasswords(
            SafetyHubExpandablePreference preference, PropertyModel model) {
        Context context = preference.getContext();
        int reusedPasswordsCount = model.get(SafetyHubModuleProperties.REUSED_PASSWORDS_COUNT);
        preference.setTitle(context.getString(R.string.safety_hub_reused_weak_passwords_title));
        preference.setSummary(
                context.getResources()
                        .getQuantityString(
                                R.plurals.safety_hub_reused_passwords_summary,
                                reusedPasswordsCount,
                                reusedPasswordsCount));
        preference.setPrimaryButtonText(
                context.getString(R.string.safety_hub_passwords_navigation_button));
        preference.setPrimaryButtonClickListener(
                model.get(SafetyHubModuleProperties.SAFE_STATE_BUTTON_LISTENER));
        preference.setSecondaryButtonText(null);
        preference.setSecondaryButtonClickListener(null);
    }

    // Updates `preference` for the password module of type {@link
    // ModuleType.UNAVAILABLE_COMPROMISED_NO_WEAK_REUSED_PASSWORDS}.
    private static void updatePreferenceForUnavailableCompromisedNoWeakReusePasswords(
            SafetyHubExpandablePreference preference, PropertyModel model) {
        Context context = preference.getContext();
        preference.setTitle(context.getString(R.string.safety_hub_no_reused_weak_passwords_title));
        preference.setSummary(
                context.getString(
                        R.string
                                .safety_hub_unavailable_compromised_no_reused_weak_passwords_summary));
        preference.setPrimaryButtonText(null);
        preference.setPrimaryButtonClickListener(null);
        preference.setSecondaryButtonText(
                context.getString(R.string.safety_hub_passwords_navigation_button));
        preference.setSecondaryButtonClickListener(
                model.get(SafetyHubModuleProperties.SAFE_STATE_BUTTON_LISTENER));
    }

    // Overrides summary and primary button fields of `preference` if passwords are controlled by a
    // policy.
    private static void overridePreferenceForManaged(
            SafetyHubExpandablePreference preference, PropertyModel model) {
        assert model.get(SafetyHubModuleProperties.IS_CONTROLLED_BY_POLICY);
        preference.setSummary(
                preference
                        .getContext()
                        .getString(R.string.safety_hub_no_passwords_summary_managed));
        String primaryButtonText = preference.getPrimaryButtonText();
        if (primaryButtonText != null) {
            assert preference.getSecondaryButtonText() == null;
            preference.setSecondaryButtonText(primaryButtonText);
            preference.setSecondaryButtonClickListener(
                    model.get(SafetyHubModuleProperties.SAFE_STATE_BUTTON_LISTENER));
            preference.setPrimaryButtonText(null);
            preference.setSecondaryButtonClickListener(null);
        }
    }

    // Updates `preference` for the password check module with the title, summary, primary button
    // text, secondary button text and their listeners.
    public static void updatePreference(
            SafetyHubExpandablePreference preference, PropertyModel model) {
        @ModuleType int type = getModuleType(model);
        switch (type) {
            case ModuleType.SIGNED_OUT:
                updatePreferenceForSignedOut(preference, model);
                break;
            case ModuleType.UNAVAILABLE_PASSWORDS:
                updatePreferenceForUnavailablePasswords(preference, model);
                break;
            case ModuleType.NO_SAVED_PASSWORDS:
                updatePreferenceForNoSavedPasswords(preference, model);
                break;
            case ModuleType.HAS_COMPROMISED_PASSWORDS:
                updatePreferenceForHasCompromisedPasswords(preference, model);
                break;
            case ModuleType.NO_COMPROMISED_PASSWORDS:
                updatePreferenceForNoCompromisedPasswords(preference, model);
                break;
            case ModuleType.HAS_WEAK_PASSWORDS:
                updatePreferenceForHasWeakPasswords(preference, model);
                break;
            case ModuleType.HAS_REUSED_PASSWORDS:
                updatePreferenceForHasReusedPasswords(preference, model);
                break;
            case ModuleType.UNAVAILABLE_COMPROMISED_NO_WEAK_REUSED_PASSWORDS:
                updatePreferenceForUnavailableCompromisedNoWeakReusePasswords(preference, model);
                break;
            default:
                throw new IllegalArgumentException();
        }

        if (model.get(SafetyHubModuleProperties.IS_CONTROLLED_BY_POLICY)) {
            overridePreferenceForManaged(preference, model);
        }
    }

    // Returns the password check module state.
    public static @ModuleState int getModuleState(PropertyModel model) {
        @ModuleType int type = getModuleType(model);
        switch (type) {
            case ModuleType.NO_SAVED_PASSWORDS:
            case ModuleType.HAS_WEAK_PASSWORDS:
            case ModuleType.HAS_REUSED_PASSWORDS:
                return ModuleState.INFO;
            case ModuleType.SIGNED_OUT:
            case ModuleType.UNAVAILABLE_PASSWORDS:
            case ModuleType.UNAVAILABLE_COMPROMISED_NO_WEAK_REUSED_PASSWORDS:
                return ModuleState.UNAVAILABLE;
            case ModuleType.HAS_COMPROMISED_PASSWORDS:
                return ModuleState.WARNING;
            case ModuleType.NO_COMPROMISED_PASSWORDS:
                return ModuleState.SAFE;
            default:
                throw new IllegalArgumentException();
        }
    }
}
