// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.preferences.autofill_assistant.AutofillAssistantPreferences;

/** Autofill Assistant related preferences util class. */
class AutofillAssistantPreferencesUtil {
    // Avoid instatiation by accident.
    private AutofillAssistantPreferencesUtil() {}

    /** Peference keeping track of whether the onboarding has been accepted. */
    @VisibleForTesting
    static final String AUTOFILL_ASSISTANT_ONBOARDING_ACCEPTED =
            "AUTOFILL_ASSISTANT_ONBOARDING_ACCEPTED";

    /** LEGACY preference for when the `do not show again' checkbox still existed. */
    @VisibleForTesting
    static final String AUTOFILL_ASSISTANT_SKIP_INIT_SCREEN = "AUTOFILL_ASSISTANT_SKIP_INIT_SCREEN";

    /** Checks whether the Autofill Assistant switch preference in settings is on. */
    static boolean isAutofillAssistantSwitchOn() {
        return ContextUtils.getAppSharedPreferences().getBoolean(
                AutofillAssistantPreferences.PREF_AUTOFILL_ASSISTANT_SWITCH, true);
    }

    /** Checks whether the Autofill Assistant onboarding has been accepted. */
    static boolean isAutofillOnboardingAccepted() {
        return ContextUtils.getAppSharedPreferences().getBoolean(
                       AUTOFILL_ASSISTANT_ONBOARDING_ACCEPTED, false)
                ||
                /* Legacy treatment: users of earlier versions should not have to see the onboarding
                again if they checked the `do not show again' checkbox*/
                ContextUtils.getAppSharedPreferences().getBoolean(
                        AUTOFILL_ASSISTANT_SKIP_INIT_SCREEN, false);
    }

    /** Checks whether the Autofill Assistant onboarding screen should be shown. */
    static boolean getShowOnboarding() {
        return !isAutofillAssistantSwitchOn() || !isAutofillOnboardingAccepted();
    }

    /**
     * Sets preferences from the initial screen.
     *
     * @param accept Flag indicating whether the ToS have been accepted.
     */
    static void setInitialPreferences(boolean accept) {
        ContextUtils.getAppSharedPreferences()
                .edit()
                .putBoolean(AutofillAssistantPreferences.PREF_AUTOFILL_ASSISTANT_SWITCH, accept)
                .apply();
        ContextUtils.getAppSharedPreferences()
                .edit()
                .putBoolean(AUTOFILL_ASSISTANT_ONBOARDING_ACCEPTED, accept)
                .apply();
    }
}
