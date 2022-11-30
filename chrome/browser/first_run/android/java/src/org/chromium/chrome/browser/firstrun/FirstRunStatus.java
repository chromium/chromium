// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;

/**
 * Gets and sets preferences related to the status of the first run experience.
 */
public class FirstRunStatus {
    // Whether the first run flow is triggered in the current browser session.
    private static boolean sFirstRunTriggered;

    /** @param triggered whether the first run flow is triggered in the current browser session. */
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public static void setFirstRunTriggered(boolean triggered) {
        sFirstRunTriggered = triggered;
    }

    /** @return whether first run flow is triggered in the current browser session. */
    public static boolean isFirstRunTriggered() {
        return sFirstRunTriggered;
    }

    /**
     * Sets the "main First Run Experience flow complete" preference.
     * @param isComplete Whether the main First Run Experience flow is complete
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public static void setFirstRunFlowComplete(boolean isComplete) {
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.FIRST_RUN_FLOW_COMPLETE, isComplete);
    }

    /**
     * Returns whether the main First Run Experience flow is complete.
     * Note: that might NOT include "intro"/"what's new" pages, but it always
     * includes ToS and Sign In pages if necessary.
     */
    public static boolean getFirstRunFlowComplete() {
        return SharedPreferencesManager.getInstance().readBoolean(
                ChromePreferenceKeys.FIRST_RUN_FLOW_COMPLETE, false);
    }

    /**
     * Sets the preference to skip the welcome page from the main First Run Experience.
     * @param isSkip Whether the welcome page should be skipped.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public static void setSkipWelcomePage(boolean isSkip) {
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.FIRST_RUN_SKIP_WELCOME_PAGE, isSkip);
    }

    /**
     * Checks whether the welcome page should be skipped from the main First Run Experience.
     */
    public static boolean shouldSkipWelcomePage() {
        return SharedPreferencesManager.getInstance().readBoolean(
                ChromePreferenceKeys.FIRST_RUN_SKIP_WELCOME_PAGE, false);
    }

    /**
     * Sets the "lightweight First Run Experience flow complete" preference.
     * @param isComplete Whether the lightweight First Run Experience flow is complete
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public static void setLightweightFirstRunFlowComplete(boolean isComplete) {
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.FIRST_RUN_LIGHTWEIGHT_FLOW_COMPLETE, isComplete);
    }

    /**
     * Returns whether the "lightweight First Run Experience flow" is complete.
     */
    public static boolean getLightweightFirstRunFlowComplete() {
        return SharedPreferencesManager.getInstance().readBoolean(
                ChromePreferenceKeys.FIRST_RUN_LIGHTWEIGHT_FLOW_COMPLETE, false);
    }

    /**
     * Sets the "First Run Experience is skipped by policy" preference. The value of this shared
     * preference is used to speed up the decision if first run is needed during start up.
     *
     * This shared preference could be updated from "true" to "false" if policy is ever unset, but
     * not vise versa. Thus, its value could be stale and cannot used to determine if the FRE was
     * actually shown in current session.
     *
     * @param isSkipped Whether the lightweight First Run Experience flow is skipped by policy.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public static void setFirstRunSkippedByPolicy(boolean isSkipped) {
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.FIRST_RUN_SKIPPED_BY_POLICY, isSkipped);
    }

    /**
     * @return Whether the First Run Experience is skipped by policy.
     * @see #setFirstRunSkippedByPolicy
     * */
    public static boolean isFirstRunSkippedByPolicy() {
        return SharedPreferencesManager.getInstance().readBoolean(
                ChromePreferenceKeys.FIRST_RUN_SKIPPED_BY_POLICY, false);
    }
}
