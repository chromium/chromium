// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.app.Activity;
import android.content.Intent;
import android.os.Build;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.IntentHandler.ExternalAppId;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.UnifiedConsentServiceBridge;

/**
 * Chrome specific dependency methods used by the Autofill Assistant outside of it's module.
 */
public class AssistantDependencyUtilsChrome {
    /**
     * Returns whether the activity was launched by GSA.
     * */
    public static boolean isGsa(@Nullable Activity activity) {
        // This can fail for certain tabs (e.g., hidden background tabs).
        if (activity == null) {
            return false;
        }

        Intent intent = activity.getIntent();
        if (intent == null) {
            // This should never happen, this is just a failsafe.
            return false;
        }

        // TODO(crbug.com/1139479): Once determineExternalIntentSource() is moved to //components
        // remove the injection.
        return IntentHandler.determineExternalIntentSource(intent) == ExternalAppId.GSA;
    }

    /**
     * Checks whether direct actions provided by Autofill Assistant should be available - assuming
     * that direct actions are available at all.
     */
    public static boolean areDirectActionsAvailable(@ActivityType int activityType) {
        return Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q
                && (activityType == ActivityType.CUSTOM_TAB || activityType == ActivityType.TABBED)
                && AssistantFeatures.AUTOFILL_ASSISTANT.isEnabled()
                && AssistantFeatures.AUTOFILL_ASSISTANT_DIRECT_ACTIONS.isEnabled();
    }

    public static boolean isMakeSearchesAndBrowsingBetterSettingEnabled() {
        return UnifiedConsentServiceBridge.isUrlKeyedAnonymizedDataCollectionEnabled(
                Profile.getLastUsedRegularProfile());
    }

    private AssistantDependencyUtilsChrome() {}
}
