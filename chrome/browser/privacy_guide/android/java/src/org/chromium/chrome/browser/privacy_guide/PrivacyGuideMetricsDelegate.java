// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_guide;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;

/**
 * A delegate class to record metrics associated with each card inside
 * Privacy Guide {@link PrivacyGuideFragment}.
 */
class PrivacyGuideMetricsDelegate {
    /**
     * Initial state of the MSBB when {@link MSBBFragment} is created.
     */
    private Boolean mInitialMsbbState;

    /**
     * A method to record metrics on the next click of {@link MSBBFragment}
     */
    private void recordMetricsOnNextForMSBBCard() {
        assert mInitialMsbbState != null : "Initial state of MSSB not set.";

        boolean currentValue = PrivacyGuideUtils.isMsbbEnabled();
        @PrivacyGuideSettingsStates
        int stateChange;

        if (mInitialMsbbState && currentValue) {
            stateChange = PrivacyGuideSettingsStates.MSBB_ON_TO_ON;
        } else if (mInitialMsbbState && !currentValue) {
            stateChange = PrivacyGuideSettingsStates.MSBB_ON_TO_OFF;
        } else if (!mInitialMsbbState && currentValue) {
            stateChange = PrivacyGuideSettingsStates.MSBB_OFF_TO_ON;
        } else {
            stateChange = PrivacyGuideSettingsStates.MSBB_OFF_TO_OFF;
        }

        // Record histogram comparing |mInitialMsbbState| and |currentValue|
        RecordHistogram.recordEnumeratedHistogram("Settings.PrivacyGuide.SettingsStates",
                stateChange, PrivacyGuideSettingsStates.MAX_VALUE);
        // Record user action for clicking the next button on the MSBB card
        RecordUserAction.record("Settings.PrivacyGuide.NextClickMSBB");
    }

    /**
     * A method to set the initial state of a card {@link PrivacyGuideFragment.FragmentType} in
     * Privacy Guide.
     * TODO(crbug.com/1238896): Support for other fragment types (SYNC, SAFE_BROWSING, COOKIES)
     *
     * @param fragmentType A privacy guide {@link PrivacyGuideFragment.FragmentType}.
     */
    void setInitialStateForCard(@PrivacyGuideFragment.FragmentType int fragmentType) {
        switch (fragmentType) {
            case PrivacyGuideFragment.FragmentType.MSBB: {
                mInitialMsbbState = PrivacyGuideUtils.isMsbbEnabled();
                break;
            }
        }
    }

    /**
     * A method to record metrics on the next click of a card {@link
     * PrivacyGuideFragment.FragmentType} in Privacy Guide.
     * TODO(crbug.com/1238896): Support for other fragment types (SYNC, SAFE_BROWSING, COOKIES)
     *
     * @param fragmentType A privacy guide {@link PrivacyGuideFragment.FragmentType}.
     */
    void recordMetricsOnNextForCard(@PrivacyGuideFragment.FragmentType int fragmentType) {
        switch (fragmentType) {
            case PrivacyGuideFragment.FragmentType.MSBB: {
                recordMetricsOnNextForMSBBCard();
                break;
            }
        }
    }

    /**
     * A method to record metrics on the next click of the privacy guide welcome page.
     */
    static void recordMetricsForWelcomeCard() {
        RecordUserAction.record("Settings.PrivacyGuide.NextClickWelcome");
    }

    /**
     * A method to record metrics for the done click of the privacy guide completion page.
     */
    static void recordMetricsForDoneButton() {
        RecordUserAction.record("Settings.PrivacyGuide.NextClickCompletion");
    }
}
