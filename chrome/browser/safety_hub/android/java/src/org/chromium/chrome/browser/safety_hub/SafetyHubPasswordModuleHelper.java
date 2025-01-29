// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import androidx.annotation.IntDef;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.safety_hub.DeprecatedSafetyHubModuleProperties.ModuleState;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

// Helper class for the Safety Hub Password Module. It allows its clients to easily update the
// preference with characteristics of the module, such as title, summary and buttons.
// TODO(https://crbug.com/388788381): Delete this class in favor of
// SafetyHubAccountPasswordsModuleMediator once the DeprecatedSafetyHubModuleViewBinder stops
// existing.
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
                model.get(DeprecatedSafetyHubModuleProperties.COMPROMISED_PASSWORDS_COUNT);
        int weakPasswordsCount =
                model.get(DeprecatedSafetyHubModuleProperties.WEAK_PASSWORDS_COUNT);
        int reusedPasswordsCount =
                model.get(DeprecatedSafetyHubModuleProperties.REUSED_PASSWORDS_COUNT);
        int totalPasswordsCount =
                model.get(DeprecatedSafetyHubModuleProperties.TOTAL_PASSWORDS_COUNT);
        boolean isSignedOut = !model.get(DeprecatedSafetyHubModuleProperties.IS_SIGNED_IN);

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
