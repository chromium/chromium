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
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.autofill_assistant.AssistantFeatures;
import org.chromium.components.autofill_assistant.AssistantTabObserver;
import org.chromium.ui.base.WindowAndroid;

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

    public static void attachTabObserver(Tab tab, AssistantTabObserver tabObserver) {
        tab.addObserver(new EmptyTabObserver() {
            @Override
            public void onActivityAttachmentChanged(
                    Tab tab, @Nullable WindowAndroid windowAndroid) {
                tabObserver.onActivityAttachmentChanged(
                        tab != null ? tab.getWebContents() : null, windowAndroid);
            }

            @Override
            public void onContentChanged(Tab tab) {
                tabObserver.onContentChanged(tab.getWebContents());
            }

            @Override
            public void onWebContentsSwapped(Tab tab, boolean didStartLoad, boolean didFinishLoad) {
                tabObserver.onWebContentsSwapped(tab.getWebContents(), didStartLoad, didFinishLoad);
            }

            @Override
            public void onDestroyed(Tab tab) {
                tabObserver.onDestroyed(tab.getWebContents());
            }

            @Override
            public void onInteractabilityChanged(Tab tab, boolean isInteractable) {
                tabObserver.onInteractabilityChanged(tab.getWebContents(), isInteractable);
            }
        });
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

    private AssistantDependencyUtilsChrome() {}
}
