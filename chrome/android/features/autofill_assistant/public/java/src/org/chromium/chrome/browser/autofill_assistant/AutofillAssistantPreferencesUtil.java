// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;

/** Autofill Assistant related preferences util class. */
public class AutofillAssistantPreferencesUtil {
    /**
     * If a user explicitly cancels a lite script >= this number, they will implicitly opt-out of
     * this experience and never see a lite script again. Note: this is only temporarily in place
     * until we have a better and more user-friendly solution, see crbug.com/1110887.
     */
    private static final int LITE_SCRIPT_MAX_NUM_CANCELED_TO_OPT_OUT = 2;

    // Avoid instantiation by accident.
    private AutofillAssistantPreferencesUtil() {}

    /** Checks whether the Autofill Assistant switch preference in settings is on. */
    static boolean isAutofillAssistantSwitchOn() {
        return SharedPreferencesManager.getInstance().readBoolean(
                ChromePreferenceKeys.AUTOFILL_ASSISTANT_ENABLED, true);
    }

    /** Checks whether proactive help is enabled. */
    public static boolean isProactiveHelpOn() {
        return isProactiveHelpSwitchOn() && isAutofillAssistantSwitchOn();
    }

    /**
     * Checks whether the proactive help switch preference in settings is on.
     * Warning: even if the switch is on, it can appear disabled if the Autofill Assistant switch is
     * off. Use {@link #isProactiveHelpOn()} to determine whether to trigger proactive help.
     */
    private static boolean isProactiveHelpSwitchOn() {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.AUTOFILL_ASSISTANT_PROACTIVE_HELP)) {
            return false;
        }

        return SharedPreferencesManager.getInstance().readBoolean(
                ChromePreferenceKeys.AUTOFILL_ASSISTANT_PROACTIVE_HELP, true);
    }

    /** Enables or disables the proactive help setting. */
    public static void setProactiveHelpSwitch(boolean enabled) {
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.AUTOFILL_ASSISTANT_PROACTIVE_HELP, enabled);
    }

    /** Returns whether the user has seen a trigger script before or not. */
    public static boolean isAutofillAssistantFirstTimeTriggerScriptUser() {
        return SharedPreferencesManager.getInstance().readBoolean(
                ChromePreferenceKeys.AUTOFILL_ASSISTANT_FIRST_TIME_LITE_SCRIPT_USER, true);
    }

    /** Marks a user as having seen a trigger script at least once before. */
    public static void setAutofillAssistantFirstTimeTriggerScriptUser(boolean firstTimeUser) {
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.AUTOFILL_ASSISTANT_FIRST_TIME_LITE_SCRIPT_USER, firstTimeUser);
    }

    /** Returns the number of times a user has explicitly canceled a lite script. */
    private static int getAutofillAssistantNumberOfLiteScriptsCanceled() {
        return SharedPreferencesManager.getInstance().readInt(
                ChromePreferenceKeys.AUTOFILL_ASSISTANT_NUMBER_OF_LITE_SCRIPTS_CANCELED, 0);
    }

    /**
     * Returns whether the user has explicitly canceled the lite script at least {@code
     * LITE_SCRIPT_MAX_NUM_CANCELED_TO_OPT_OUT} times.
     */
    public static boolean isAutofillAssistantLiteScriptCancelThresholdReached() {
        return getAutofillAssistantNumberOfLiteScriptsCanceled()
                >= LITE_SCRIPT_MAX_NUM_CANCELED_TO_OPT_OUT;
    }

    /** Increments the number of times a user has explicitly canceled a lite script. */
    public static void incrementAutofillAssistantNumberOfLiteScriptsCanceled() {
        int numCanceled = getAutofillAssistantNumberOfLiteScriptsCanceled() + 1;
        SharedPreferencesManager sharedPreferencesManager = SharedPreferencesManager.getInstance();
        sharedPreferencesManager.writeInt(
                ChromePreferenceKeys.AUTOFILL_ASSISTANT_NUMBER_OF_LITE_SCRIPTS_CANCELED,
                numCanceled);
        if (isAutofillAssistantLiteScriptCancelThresholdReached()
                && !sharedPreferencesManager.contains(
                        ChromePreferenceKeys.AUTOFILL_ASSISTANT_ENABLED)) {
            // Disable the flag, such that users will not see the lite script again. This will also
            // create the setting in the Chrome settings, if it was not present before, which will
            // allow users to opt back in.
            sharedPreferencesManager.writeBoolean(
                    ChromePreferenceKeys.AUTOFILL_ASSISTANT_ENABLED, false);
        }
    }

    /** Checks whether the Autofill Assistant onboarding has been accepted. */
    public static boolean isAutofillOnboardingAccepted() {
        return SharedPreferencesManager.getInstance().readBoolean(
                       ChromePreferenceKeys.AUTOFILL_ASSISTANT_ONBOARDING_ACCEPTED, false)
                ||
                /* Legacy treatment: users of earlier versions should not have to see the onboarding
                again if they checked the `do not show again' checkbox*/
                SharedPreferencesManager.getInstance().readBoolean(
                        ChromePreferenceKeys.AUTOFILL_ASSISTANT_SKIP_INIT_SCREEN, false);
    }

    /** Checks whether the Autofill Assistant onboarding screen should be shown. */
    public static boolean getShowOnboarding() {
        if (ChromeFeatureList.isEnabled(
                    ChromeFeatureList.AUTOFILL_ASSISTANT_DISABLE_ONBOARDING_FLOW)) {
            return false;
        }
        return !isAutofillAssistantSwitchOn() || !isAutofillOnboardingAccepted();
    }

    /**
     * Sets preferences from the initial screen.
     *
     * @param accept Flag indicating whether the ToS have been accepted.
     */
    public static void setInitialPreferences(boolean accept) {
        if (accept) {
            SharedPreferencesManager.getInstance().writeBoolean(
                    ChromePreferenceKeys.AUTOFILL_ASSISTANT_ENABLED, accept);
        }
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.AUTOFILL_ASSISTANT_ONBOARDING_ACCEPTED, accept);
    }
}
