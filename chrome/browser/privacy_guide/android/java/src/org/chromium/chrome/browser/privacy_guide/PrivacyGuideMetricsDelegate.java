// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_guide;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingState;
import org.chromium.components.content_settings.CookieControlsMode;

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

    /**
     * A method to record metrics on MSBB toggle change of the Privacy Guide's {@link MSBBFragment}.
     */
    static void recordMetricsOnMSBBChange(boolean isMSBBOn) {
        if (isMSBBOn) {
            RecordUserAction.record("Settings.PrivacyGuide.ChangeMSBBOn");
        } else {
            RecordUserAction.record("Settings.PrivacyGuide.ChangeMSBBOff");
        }
    }

    /**
     * A method to record metrics on the History Sync toggle change of the Privacy Guide's {@link
     * SyncFragment}.
     */
    static void recordMetricsOnSyncChange(boolean isHistorySyncOn) {
        if (isHistorySyncOn) {
            RecordUserAction.record("Settings.PrivacyGuide.ChangeHistorySyncOn");
        } else {
            RecordUserAction.record("Settings.PrivacyGuide.ChangeHistorySyncOff");
        }
    }

    /**
     * A method to record metrics on Safe Browsing radio button change of the Privacy Guide's {@link
     * SafeBrowsingFragment}.
     */
    static void recordMetricsOnSafeBrowsingChange(@SafeBrowsingState int safeBrowsingState) {
        switch (safeBrowsingState) {
            case SafeBrowsingState.ENHANCED_PROTECTION:
                RecordUserAction.record("Settings.PrivacyGuide.ChangeSafeBrowsingEnhanced");
                break;
            case SafeBrowsingState.STANDARD_PROTECTION:
                RecordUserAction.record("Settings.PrivacyGuide.ChangeSafeBrowsingStandard");
                break;
            default:
                assert false : "Unexpected SafeBrowsingState " + safeBrowsingState;
        }
    }

    /**
     * A method to record metrics on Cookie Controls radio button change of the Privacy Guide's
     * {@link CookiesFragment}.
     */
    static void recordMetricsOnCookieControlsChange(@CookieControlsMode int cookieControlsMode) {
        switch (cookieControlsMode) {
            case CookieControlsMode.INCOGNITO_ONLY:
                RecordUserAction.record("Settings.PrivacyGuide.ChangeCookiesBlock3PIncognito");
                break;
            case CookieControlsMode.BLOCK_THIRD_PARTY:
                RecordUserAction.record("Settings.PrivacyGuide.ChangeCookiesBlock3P");
                break;
            default:
                assert false : "Unexpected CookieControlMode " + cookieControlsMode;
        }
    }

    /**
     * A method to record metrics on the back click of a card {@link
     * PrivacyGuideFragment.FragmentType} in Privacy Guide.
     * TODO(crbug.com/1238896): Support for other fragment types (SAFE_BROWSING, COOKIES)
     *
     * @param fragmentType A privacy guide {@link PrivacyGuideFragment.FragmentType}.
     */
    static void recordMetricsOnBackForCard(@PrivacyGuideFragment.FragmentType int fragmentType) {
        switch (fragmentType) {
            case PrivacyGuideFragment.FragmentType.SYNC: {
                RecordUserAction.record("Settings.PrivacyGuide.BackClickHistorySync");
            }
        }
    }
}
