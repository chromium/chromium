// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;

/**
 * Gets and sets preferences related to the status of the Vr feedback infobar.
 */
public class VrFeedbackStatus {
    private static final String FEEDBACK_FREQUENCY_PARAM_NAME = "feedback_frequency";
    private static final int DEFAULT_FEEDBACK_FREQUENCY = 10;

    /**
     * Returns how often we should show the feedback prompt.
     */
    public static int getFeedbackFrequency() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.VR_BROWSING_FEEDBACK, FEEDBACK_FREQUENCY_PARAM_NAME,
                DEFAULT_FEEDBACK_FREQUENCY);
    }

    /**
     * Sets the "opted out of entering VR feedback" preference.
     * @param optOut Whether the VR feedback option has been opted-out of.
     */
    public static void setFeedbackOptOut(boolean optOut) {
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.VR_FEEDBACK_OPT_OUT, optOut);
    }

    /**
     * Returns whether the user opted out of entering feedback for their VR experience.
     */
    public static boolean getFeedbackOptOut() {
        return SharedPreferencesManager.getInstance().readBoolean(
                ChromePreferenceKeys.VR_FEEDBACK_OPT_OUT, false);
    }

    /**
     * Sets the "exited VR mode into 2D browsing" preference.
     * @param count The number of times the user exited VR and entered 2D browsing mode
     */
    public static void setUserExitedAndEntered2DCount(int count) {
        SharedPreferencesManager.getInstance().writeInt(
                ChromePreferenceKeys.VR_EXIT_TO_2D_COUNT, count);
    }

    /**
     * Returns the number of times the user has exited VR mode and entered the 2D browsing
     * mode.
     */
    public static int getUserExitedAndEntered2DCount() {
        return SharedPreferencesManager.getInstance().readInt(
                ChromePreferenceKeys.VR_EXIT_TO_2D_COUNT, 0);
    }
}
