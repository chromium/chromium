// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_guide;

import android.os.Bundle;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingState;
import org.chromium.components.content_settings.CookieControlsMode;

/**
 * A delegate class to record metrics associated with each card inside
 * Privacy Guide {@link PrivacyGuideFragment}.
 */
class PrivacyGuideMetricsDelegate {
    private static final String INITIAL_MSBB_STATE = "INITIAL_MSBB_STATE";
    private static final String INITIAL_HISTORY_SYNC_STATE = "INITIAL_HISTORY_SYNC_STATE";
    private static final String INITIAL_SAFE_BROWSING_STATE = "INITIAL_SAFE_BROWSING_STATE";
    private static final String INITIAL_COOKIES_CONTROL_MODE = "INITIAL_COOKIES_CONTROL_MODE";
    private static final String INITIAL_SEARCH_SUGGESTIONS_STATE =
            "INITIAL_SEARCH_SUGGESTIONS_STATE";
    private static final String INITIAL_AD_TOPICS_STATE = "INITIAL_AD_TOPICS_STATE";

    private final Profile mProfile;

    /** Initial state of the MSBB when {@link MSBBFragment} is created. */
    private @Nullable Boolean mInitialMsbbState;

    /** Initial state of History Sync when {@link HistorySyncFragment} is created. */
    private @Nullable Boolean mInitialHistorySyncState;

    /** Initial state of the Safe Browsing when {@link SafeBrowsingFragment} is created. */
    private @Nullable @SafeBrowsingState Integer mInitialSafeBrowsingState;

    /** Initial mode of the Cookies Control when {@link CookiesFragment} is created. */
    private @Nullable @CookieControlsMode Integer mInitialCookiesControlMode;

    /**
     * Initial state of the Search Suggestions when {@link SearchSuggestionsFragment} is created.
     */
    private @Nullable Boolean mInitialSearchSuggestionsState;

    /** Initial state of Ad topics when {@link AdTopicsFragment} is created. */
    private @Nullable Boolean mInitialAdTopicsState;

    PrivacyGuideMetricsDelegate(Profile profile) {
        mProfile = profile;
    }

    /** A method to persist the initial state of all Fragments on Activity destruction. */
    void saveState(@NonNull Bundle bundle) {
        if (mInitialMsbbState != null) {
            bundle.putBoolean(INITIAL_MSBB_STATE, mInitialMsbbState);
        }
        if (mInitialHistorySyncState != null) {
            bundle.putBoolean(INITIAL_HISTORY_SYNC_STATE, mInitialHistorySyncState);
        }
        if (mInitialSafeBrowsingState != null) {
            bundle.putInt(INITIAL_SAFE_BROWSING_STATE, mInitialSafeBrowsingState);
        }
        if (mInitialCookiesControlMode != null) {
            bundle.putInt(INITIAL_COOKIES_CONTROL_MODE, mInitialCookiesControlMode);
        }
        if (mInitialSearchSuggestionsState != null) {
            bundle.putBoolean(INITIAL_SEARCH_SUGGESTIONS_STATE, mInitialSearchSuggestionsState);
        }
        if (mInitialAdTopicsState != null) {
            bundle.putBoolean(INITIAL_AD_TOPICS_STATE, mInitialAdTopicsState);
        }
    }

    /** A method to restore the initial state of all Fragments on Activity recreation. */
    void restoreState(@NonNull Bundle bundle) {
        if (bundle.containsKey(INITIAL_MSBB_STATE)) {
            mInitialMsbbState = bundle.getBoolean(INITIAL_MSBB_STATE);
        }
        if (bundle.containsKey(INITIAL_HISTORY_SYNC_STATE)) {
            mInitialHistorySyncState = bundle.getBoolean(INITIAL_HISTORY_SYNC_STATE);
        }
        if (bundle.containsKey(INITIAL_SAFE_BROWSING_STATE)) {
            mInitialSafeBrowsingState = bundle.getInt(INITIAL_SAFE_BROWSING_STATE);
        }
        if (bundle.containsKey(INITIAL_COOKIES_CONTROL_MODE)) {
            mInitialCookiesControlMode = bundle.getInt(INITIAL_COOKIES_CONTROL_MODE);
        }
        if (bundle.containsKey(INITIAL_SEARCH_SUGGESTIONS_STATE)) {
            mInitialSearchSuggestionsState = bundle.getBoolean(INITIAL_SEARCH_SUGGESTIONS_STATE);
        }
        if (bundle.containsKey(INITIAL_AD_TOPICS_STATE)) {
            mInitialAdTopicsState = bundle.getBoolean(INITIAL_AD_TOPICS_STATE);
        }
    }

    /** A method to record metrics on the next click of {@link MSBBFragment} */
    private void recordMetricsOnNextForMSBBCard() {
        assert mInitialMsbbState != null : "Initial state of MSSB not set.";

        boolean currentValue = PrivacyGuideUtils.isMsbbEnabled(mProfile);
        @PrivacyGuideSettingsStates int stateChange;

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
        RecordHistogram.recordEnumeratedHistogram(
                "Settings.PrivacyGuide.SettingsStates",
                stateChange,
                PrivacyGuideSettingsStates.MAX_VALUE);
        // Record user action for clicking the next button on the MSBB card
        RecordUserAction.record("Settings.PrivacyGuide.NextClickMSBB");
        // Record histogram for clicking the next button on the MSBB card
        RecordHistogram.recordEnumeratedHistogram(
                "Settings.PrivacyGuide.NextNavigation",
                PrivacyGuideInteractions.MSBB_NEXT_BUTTON,
                PrivacyGuideInteractions.MAX_VALUE);
    }

    /** A method to record metrics on the next click of {@link HistorySyncFragment}. */
    private void recordMetricsOnNextForHistorySyncCard() {
        assert mInitialHistorySyncState != null : "Initial state of History Sync not set.";

        boolean currentValue = PrivacyGuideUtils.isHistorySyncEnabled(mProfile);
        @PrivacyGuideSettingsStates int stateChange;

        if (mInitialHistorySyncState && currentValue) {
            stateChange = PrivacyGuideSettingsStates.HISTORY_SYNC_ON_TO_ON;
        } else if (mInitialHistorySyncState && !currentValue) {
            stateChange = PrivacyGuideSettingsStates.HISTORY_SYNC_ON_TO_OFF;
        } else if (!mInitialHistorySyncState && currentValue) {
            stateChange = PrivacyGuideSettingsStates.HISTORY_SYNC_OFF_TO_ON;
        } else {
            stateChange = PrivacyGuideSettingsStates.HISTORY_SYNC_OFF_TO_OFF;
        }

        // Record histogram comparing |mInitialHistorySyncState| and |currentValue|
        RecordHistogram.recordEnumeratedHistogram(
                "Settings.PrivacyGuide.SettingsStates",
                stateChange,
                PrivacyGuideSettingsStates.MAX_VALUE);
        // Record user action for clicking the next button on the History Sync card
        RecordUserAction.record("Settings.PrivacyGuide.NextClickHistorySync");
        // Record histogram for clicking the next button on the History Sync card
        RecordHistogram.recordEnumeratedHistogram(
                "Settings.PrivacyGuide.NextNavigation",
                PrivacyGuideInteractions.HISTORY_SYNC_NEXT_BUTTON,
                PrivacyGuideInteractions.MAX_VALUE);
    }

    /** A method to record metrics on the next click of {@link SafeBrowsingFragment} */
    private void recordMetricsOnNextForSafeBrowsingCard(Profile profile) {
        assert mInitialSafeBrowsingState != null : "Initial state of Safe Browsing not set.";

        @SafeBrowsingState int currentValue = PrivacyGuideUtils.getSafeBrowsingState(profile);

        boolean isStartStateEnhance =
                mInitialSafeBrowsingState == SafeBrowsingState.ENHANCED_PROTECTION;
        boolean isEndStateEnhance = currentValue == SafeBrowsingState.ENHANCED_PROTECTION;

        @PrivacyGuideSettingsStates int stateChange;

        if (isStartStateEnhance && isEndStateEnhance) {
            stateChange = PrivacyGuideSettingsStates.SAFE_BROWSING_ENHANCED_TO_ENHANCED;
        } else if (isStartStateEnhance && !isEndStateEnhance) {
            stateChange = PrivacyGuideSettingsStates.SAFE_BROWSING_ENHANCED_TO_STANDARD;
        } else if (!isStartStateEnhance && isEndStateEnhance) {
            stateChange = PrivacyGuideSettingsStates.SAFE_BROWSING_STANDARD_TO_ENHANCED;
        } else {
            stateChange = PrivacyGuideSettingsStates.SAFE_BROWSING_STANDARD_TO_STANDARD;
        }

        // Record histogram comparing |mInitialSafeBrowsingState| and |currentValue|
        RecordHistogram.recordEnumeratedHistogram(
                "Settings.PrivacyGuide.SettingsStates",
                stateChange,
                PrivacyGuideSettingsStates.MAX_VALUE);
        // Record user action for clicking the next button on the Safe Browsing card
        RecordUserAction.record("Settings.PrivacyGuide.NextClickSafeBrowsing");
        // Record histogram for clicking the next button on the Safe Browsing card
        RecordHistogram.recordEnumeratedHistogram(
                "Settings.PrivacyGuide.NextNavigation",
                PrivacyGuideInteractions.SAFE_BROWSING_NEXT_BUTTON,
                PrivacyGuideInteractions.MAX_VALUE);
    }

    /** A method to record metrics on the next click of {@link CookiesFragment} */
    private void recordMetricsOnNextForCookiesCard() {
        assert mInitialCookiesControlMode != null : "Initial mode of Cookie Control not set.";

        @CookieControlsMode int currentValue = PrivacyGuideUtils.getCookieControlsMode(mProfile);

        boolean isInitialStateBlock3PIncognito =
                mInitialCookiesControlMode == CookieControlsMode.INCOGNITO_ONLY;
        boolean isEndStateBlock3PIncognito = currentValue == CookieControlsMode.INCOGNITO_ONLY;

        @PrivacyGuideSettingsStates int stateChange;

        if (isInitialStateBlock3PIncognito && isEndStateBlock3PIncognito) {
            stateChange = PrivacyGuideSettingsStates.BLOCK3P_INCOGNITO_TO3P_INCOGNITO;
        } else if (isInitialStateBlock3PIncognito && !isEndStateBlock3PIncognito) {
            stateChange = PrivacyGuideSettingsStates.BLOCK3P_INCOGNITO_TO3P;
        } else if (!isInitialStateBlock3PIncognito && isEndStateBlock3PIncognito) {
            stateChange = PrivacyGuideSettingsStates.BLOCK3P_TO3P_INCOGNITO;
        } else {
            stateChange = PrivacyGuideSettingsStates.BLOCK3P_TO3P;
        }

        // Record histogram comparing |mInitialCookiesControlMode| and |currentValue|
        RecordHistogram.recordEnumeratedHistogram(
                "Settings.PrivacyGuide.SettingsStates",
                stateChange,
                PrivacyGuideSettingsStates.MAX_VALUE);
        // Record user action for clicking the next button on the Cookies card
        RecordUserAction.record("Settings.PrivacyGuide.NextClickCookies");
        // Record histogram for clicking the next button on the Cookies card
        RecordHistogram.recordEnumeratedHistogram(
                "Settings.PrivacyGuide.NextNavigation",
                PrivacyGuideInteractions.COOKIES_NEXT_BUTTON,
                PrivacyGuideInteractions.MAX_VALUE);
    }

    /** A method to record metrics on the next click of {@link SearchSuggestionsFragment} */
    private void recordMetricsOnNextForSearchSuggestionsCard() {
        assert mInitialSearchSuggestionsState != null
                : "Initial state of search suggestions not set.";

        boolean currentValue = PrivacyGuideUtils.isSearchSuggestionsEnabled(mProfile);
        @PrivacyGuideSettingsStates int stateChange;

        if (mInitialSearchSuggestionsState && currentValue) {
            stateChange = PrivacyGuideSettingsStates.SEARCH_SUGGESTIONS_ON_TO_ON;
        } else if (mInitialSearchSuggestionsState && !currentValue) {
            stateChange = PrivacyGuideSettingsStates.SEARCH_SUGGESTIONS_ON_TO_OFF;
        } else if (!mInitialSearchSuggestionsState && currentValue) {
            stateChange = PrivacyGuideSettingsStates.SEARCH_SUGGESTIONS_OFF_TO_ON;
        } else {
            stateChange = PrivacyGuideSettingsStates.SEARCH_SUGGESTIONS_OFF_TO_OFF;
        }

        // Record histogram comparing |mInitialSearchSuggestionsState| and |currentValue|
        RecordHistogram.recordEnumeratedHistogram(
                "Settings.PrivacyGuide.SettingsStates",
                stateChange,
                PrivacyGuideSettingsStates.MAX_VALUE);
        // Record user action for clicking the next button on the search suggestions card
        RecordUserAction.record("Settings.PrivacyGuide.NextClickSearchSuggestions");
        // Record histogram for clicking the next button on the search suggestions card
        RecordHistogram.recordEnumeratedHistogram(
                "Settings.PrivacyGuide.NextNavigation",
                PrivacyGuideInteractions.SEARCH_SUGGESTIONS_NEXT_BUTTON,
                PrivacyGuideInteractions.MAX_VALUE);
    }

    /** A method to record metrics on the next click of {@link AdTopicsFragment} */
    private void recordMetricsOnNextForAdTopicsCard() {
        assert mInitialAdTopicsState != null : "Initial state of Ad Topics not set.";

        boolean currentValue = PrivacyGuideUtils.isAdTopicsEnabled(mProfile);
        @PrivacyGuideSettingsStates int stateChange;

        if (mInitialAdTopicsState && currentValue) {
            stateChange = PrivacyGuideSettingsStates.AD_TOPICS_ON_TO_ON;
        } else if (mInitialAdTopicsState && !currentValue) {
            stateChange = PrivacyGuideSettingsStates.AD_TOPICS_ON_TO_OFF;
        } else if (!mInitialAdTopicsState && currentValue) {
            stateChange = PrivacyGuideSettingsStates.AD_TOPICS_OFF_TO_ON;
        } else {
            stateChange = PrivacyGuideSettingsStates.AD_TOPICS_OFF_TO_OFF;
        }

        // Record histogram comparing |mInitialAdTopicsState| and |currentValue|
        RecordHistogram.recordEnumeratedHistogram(
                "Settings.PrivacyGuide.SettingsStates",
                stateChange,
                PrivacyGuideSettingsStates.MAX_VALUE);
        // Record user action for clicking the next button on the AdTopics card
        RecordUserAction.record("Settings.PrivacyGuide.NextClickAdTopics");
        // Record histogram for clicking the next button on the AdTopics card
        RecordHistogram.recordEnumeratedHistogram(
                "Settings.PrivacyGuide.NextNavigation",
                PrivacyGuideInteractions.AD_TOPICS_NEXT_BUTTON,
                PrivacyGuideInteractions.MAX_VALUE);
    }

    /**
     * A method to set the initial state of a card {@link PrivacyGuideFragment.FragmentType} in
     * Privacy Guide.
     *
     * @param fragmentType A privacy guide {@link PrivacyGuideFragment.FragmentType}.
     */
    void setInitialStateForCard(@PrivacyGuideFragment.FragmentType int fragmentType) {
        switch (fragmentType) {
            case PrivacyGuideFragment.FragmentType.MSBB:
                {
                    mInitialMsbbState = PrivacyGuideUtils.isMsbbEnabled(mProfile);
                    break;
                }
            case PrivacyGuideFragment.FragmentType.HISTORY_SYNC:
                {
                    mInitialHistorySyncState = PrivacyGuideUtils.isHistorySyncEnabled(mProfile);
                    break;
                }
            case PrivacyGuideFragment.FragmentType.SAFE_BROWSING:
                {
                    mInitialSafeBrowsingState = PrivacyGuideUtils.getSafeBrowsingState(mProfile);
                    break;
                }
            case PrivacyGuideFragment.FragmentType.COOKIES:
                {
                    mInitialCookiesControlMode = PrivacyGuideUtils.getCookieControlsMode(mProfile);
                    break;
                }
            case PrivacyGuideFragment.FragmentType.SEARCH_SUGGESTIONS:
                {
                    mInitialSearchSuggestionsState =
                            PrivacyGuideUtils.isSearchSuggestionsEnabled(mProfile);
                    break;
                }
            case PrivacyGuideFragment.FragmentType.PRELOAD:
                {
                    // TODO(crbug.com/40281867): Initial state for the preload card should be added
                    // here.
                    break;
                }
            case PrivacyGuideFragment.FragmentType.AD_TOPICS:
                {
                    mInitialAdTopicsState = PrivacyGuideUtils.isAdTopicsEnabled(mProfile);
                    break;
                }
            case PrivacyGuideFragment.FragmentType.WELCOME:
            case PrivacyGuideFragment.FragmentType.DONE:
                // The Welcome and Done cards don't store/update any state.
                break;
            default:
                assert false : "Unexpected fragmentType " + fragmentType;
        }
    }

    /**
     * A method to record metrics on the next click of a card {@link
     * PrivacyGuideFragment.FragmentType} in Privacy Guide.
     *
     * @param fragmentType A privacy guide {@link PrivacyGuideFragment.FragmentType}.
     */
    void recordMetricsOnNextForCard(@PrivacyGuideFragment.FragmentType int fragmentType) {
        switch (fragmentType) {
            case PrivacyGuideFragment.FragmentType.WELCOME:
                recordMetricsForWelcomeCard();
                break;
            case PrivacyGuideFragment.FragmentType.MSBB:
                {
                    recordMetricsOnNextForMSBBCard();
                    break;
                }
            case PrivacyGuideFragment.FragmentType.HISTORY_SYNC:
                {
                    recordMetricsOnNextForHistorySyncCard();
                    break;
                }
            case PrivacyGuideFragment.FragmentType.SAFE_BROWSING:
                {
                    recordMetricsOnNextForSafeBrowsingCard(mProfile);
                    break;
                }
            case PrivacyGuideFragment.FragmentType.COOKIES:
                {
                    recordMetricsOnNextForCookiesCard();
                    break;
                }
            case PrivacyGuideFragment.FragmentType.SEARCH_SUGGESTIONS:
                {
                    recordMetricsOnNextForSearchSuggestionsCard();
                    break;
                }
            case PrivacyGuideFragment.FragmentType.PRELOAD:
                {
                    // TODO(crbug.com/40281867): Metrics on next for preload card should be recorded
                    // here.
                    break;
                }
            case PrivacyGuideFragment.FragmentType.AD_TOPICS:
                {
                    recordMetricsOnNextForAdTopicsCard();
                    break;
                }
            default:
                // The Done card does not have a next button and we won't support a case for it
                assert false : "Unexpected fragmentType " + fragmentType;
        }
    }

    /** A method to record metrics on the next click of the privacy guide welcome page. */
    static void recordMetricsForWelcomeCard() {
        RecordUserAction.record("Settings.PrivacyGuide.NextClickWelcome");
        RecordHistogram.recordEnumeratedHistogram(
                "Settings.PrivacyGuide.NextNavigation",
                PrivacyGuideInteractions.WELCOME_NEXT_BUTTON,
                PrivacyGuideInteractions.MAX_VALUE);
    }

    /** A method to record metrics for the done click of the privacy guide completion page. */
    static void recordMetricsForDoneButton() {
        RecordUserAction.record("Settings.PrivacyGuide.NextClickCompletion");
        RecordHistogram.recordEnumeratedHistogram(
                "Settings.PrivacyGuide.NextNavigation",
                PrivacyGuideInteractions.COMPLETION_NEXT_BUTTON,
                PrivacyGuideInteractions.MAX_VALUE);
    }

    /**
     * A method to record metrics on the Privacy Sandbox link click on the privacy guide done page.
     */
    static void recordMetricsForPsLink() {
        RecordUserAction.record("Settings.PrivacyGuide.CompletionPSClick");
        RecordHistogram.recordEnumeratedHistogram(
                "Settings.PrivacyGuide.EntryExit",
                PrivacyGuideInteractions.PRIVACY_SANDBOX_COMPLETION_LINK,
                PrivacyGuideInteractions.MAX_VALUE);
    }

    /** A method to record metrics on the WAA link click on the privacy guide done page. */
    static void recordMetricsForWaaLink() {
        RecordUserAction.record("Settings.PrivacyGuide.CompletionSWAAClick");
        RecordHistogram.recordEnumeratedHistogram(
                "Settings.PrivacyGuide.EntryExit",
                PrivacyGuideInteractions.SWAA_COMPLETION_LINK,
                PrivacyGuideInteractions.MAX_VALUE);
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
     * HistorySyncFragment}.
     */
    static void recordMetricsOnHistorySyncChange(boolean isHistorySyncOn) {
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
     * A method to record metrics on Search Suggestions toggle change of the Privacy Guide's {@link
     * SearchSuggestionsFragment}.
     */
    static void recordMetricsOnSearchSuggestionsChange(boolean isSearchSuggestionsOn) {
        if (isSearchSuggestionsOn) {
            RecordUserAction.record("Settings.PrivacyGuide.ChangeSearchSuggestionsOn");
        } else {
            RecordUserAction.record("Settings.PrivacyGuide.ChangeSearchSuggestionsOff");
        }
    }

    /**
     * A method to record metrics on Ad Topics toggle change of the Privacy Guide's {@link
     * AdTopicsFragment}.
     */
    static void recordMetricsOnAdTopicsChange(boolean isAdTopicsOn) {
        if (isAdTopicsOn) {
            RecordUserAction.record("Settings.PrivacyGuide.ChangeAdTopicsOn");
        } else {
            RecordUserAction.record("Settings.PrivacyGuide.ChangeAdTopicsOff");
        }
    }

    /**
     * A method to record metrics on the back click of a card {@link
     * PrivacyGuideFragment.FragmentType} in Privacy Guide.
     *
     * @param fragmentType A privacy guide {@link PrivacyGuideFragment.FragmentType}.
     */
    static void recordMetricsOnBackForCard(@PrivacyGuideFragment.FragmentType int fragmentType) {
        switch (fragmentType) {
            case PrivacyGuideFragment.FragmentType.HISTORY_SYNC:
                {
                    RecordUserAction.record("Settings.PrivacyGuide.BackClickHistorySync");
                    break;
                }
            case PrivacyGuideFragment.FragmentType.SAFE_BROWSING:
                {
                    RecordUserAction.record("Settings.PrivacyGuide.BackClickSafeBrowsing");
                    break;
                }
            case PrivacyGuideFragment.FragmentType.COOKIES:
                {
                    RecordUserAction.record("Settings.PrivacyGuide.BackClickCookies");
                    break;
                }
            case PrivacyGuideFragment.FragmentType.MSBB:
                {
                    RecordUserAction.record("Settings.PrivacyGuide.BackClickMSBB");
                    break;
                }
            case PrivacyGuideFragment.FragmentType.SEARCH_SUGGESTIONS:
                {
                    RecordUserAction.record("Settings.PrivacyGuide.BackClickSearchSuggestions");
                    break;
                }
            case PrivacyGuideFragment.FragmentType.PRELOAD:
                {
                    // TODO(crbug.com/40281867): Metrics for preload card back click should be
                    // recorded here.
                    break;
                }
            case PrivacyGuideFragment.FragmentType.AD_TOPICS:
                {
                    RecordUserAction.record("Settings.PrivacyGuide.BackClickAdTopics");
                    break;
                }
            case PrivacyGuideFragment.FragmentType.DONE:
                {
                    RecordUserAction.record("Settings.PrivacyGuide.BackClickCompletion");
                    break;
                }
            default:
                // The Welcome card does not have a back button, and we won't support a case for it.
                assert false : "Unexpected fragmentType " + fragmentType;
        }
    }
}
