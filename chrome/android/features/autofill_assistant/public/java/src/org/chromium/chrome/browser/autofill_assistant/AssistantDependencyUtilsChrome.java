// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.app.Activity;
import android.content.Intent;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.IntentHandler.ExternalAppId;

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

    private AssistantDependencyUtilsChrome() {}
}
