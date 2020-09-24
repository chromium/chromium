// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import android.content.Context;
import android.net.Uri;
import android.telephony.TelephonyManager;
import android.text.TextUtils;
import android.text.format.DateUtils;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.CollectionUtil;
import org.chromium.chrome.browser.ChromeVersionInfo;
import org.chromium.chrome.browser.compositor.bottombar.contextualsearch.ContextualSearchPanel;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchFieldTrial.ContextualSearchSetting;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchFieldTrial.ContextualSearchSwitch;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchSelectionController.SelectionType;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.privacy.settings.PrivacyPreferencesManager;
import org.chromium.components.embedder_support.util.UrlConstants;

import java.net.URL;
import java.util.HashSet;
import java.util.List;
import java.util.Locale;
import java.util.regex.Pattern;

/**
 * Handles policy decisions for the {@code ContextualSearchManager}.
 */
class ContextualSearchPolicy {
    private static final Pattern CONTAINS_WHITESPACE_PATTERN = Pattern.compile("\\s");
    private static final String DOMAIN_GOOGLE = "google";
    private static final String PATH_AMP = "/amp/";
    private static final int REMAINING_NOT_APPLICABLE = -1;
    private static final int TAP_TRIGGERED_PROMO_LIMIT = 50;

    private static final HashSet<String> PREDOMINENTLY_ENGLISH_SPEAKING_COUNTRIES =
            CollectionUtil.newHashSet("GB", "US");

    private final SharedPreferencesManager mPreferencesManager;
    private final ContextualSearchSelectionController mSelectionController;
    private ContextualSearchNetworkCommunicator mNetworkCommunicator;
    private ContextualSearchPanel mSearchPanel;

    // Members used only for testing purposes.
    private boolean mDidOverrideDecidedStateForTesting;
    private boolean mDecidedStateForTesting;
    private Integer mTapTriggeredPromoLimitForTesting;

    /**
     * ContextualSearchPolicy constructor.
     */
    public ContextualSearchPolicy(ContextualSearchSelectionController selectionController,
            ContextualSearchNetworkCommunicator networkCommunicator) {
        mPreferencesManager = SharedPreferencesManager.getInstance();

        mSelectionController = selectionController;
        mNetworkCommunicator = networkCommunicator;
        if (selectionController != null) selectionController.setPolicy(this);
    }

    /**
     * Sets the handle to the ContextualSearchPanel.
     * @param panel The ContextualSearchPanel.
     */
    public void setContextualSearchPanel(ContextualSearchPanel panel) {
        mSearchPanel = panel;
    }

    /**
     * @return The number of additional times to show the promo on tap, 0 if it should not be shown,
     *         or a negative value if the counter has been disabled or the user has accepted
     *         the promo.
     */
    int getPromoTapsRemaining() {
        if (!isUserUndecided()) return REMAINING_NOT_APPLICABLE;

        // Return a non-negative value if opt-out promo counter is enabled, and there's a limit.
        DisableablePromoTapCounter counter = getPromoTapCounter();
        if (counter.isEnabled()) {
            int limit = getPromoTapTriggeredLimit();
            if (limit >= 0) return Math.max(0, limit - counter.getCount());
        }

        return REMAINING_NOT_APPLICABLE;
    }

    private int getPromoTapTriggeredLimit() {
        return mTapTriggeredPromoLimitForTesting != null
                ? mTapTriggeredPromoLimitForTesting.intValue()
                : TAP_TRIGGERED_PROMO_LIMIT;
    }

    /**
     * @return the {@link DisableablePromoTapCounter}.
     */
    DisableablePromoTapCounter getPromoTapCounter() {
        return DisableablePromoTapCounter.getInstance(mPreferencesManager);
    }

    /**
     * @return Whether a Tap gesture is currently supported as a trigger for the feature.
     */
    boolean isTapSupported() {
        if (isRelatedSearchesEnabled()) return true;

        if (isTapDisabledDueToLongpress()) return false;

        return (!isUserUndecided()
                       || ContextualSearchFieldTrial.getSwitch(
                               ContextualSearchSwitch
                                       .IS_CONTEXTUAL_SEARCH_TAP_DISABLE_OVERRIDE_ENABLED))
                ? true
                : (getPromoTapsRemaining() != 0);
    }

    /**
     * @return whether or not the Contextual Search Result should be preloaded before the user
     *         explicitly interacts with the feature.
     */
    boolean shouldPrefetchSearchResult() {
        if (isMandatoryPromoAvailable()
                || !PrivacyPreferencesManager.getInstance().getNetworkPredictionEnabled()) {
            return false;
        }

        // We never preload unless we have sent page context (done through a Resolve request).
        // Only some gestures can resolve, and only when resolve privacy rules are met.
        return (mSelectionController.getSelectionType() == SelectionType.TAP
                       || mSelectionController.getSelectionType()
                               == SelectionType.RESOLVING_LONG_PRESS)
                && shouldPreviousGestureResolve();
    }

    /**
     * @return Whether the previous gesture should resolve.
     */
    boolean shouldPreviousGestureResolve() {
        if (isMandatoryPromoAvailable()
                || ContextualSearchFieldTrial.getSwitch(
                        ContextualSearchSwitch.IS_SEARCH_TERM_RESOLUTION_DISABLED)) {
            return false;
        }

        // The user must have decided on privacy to resolve page content on HTTPS.
        return !isUserUndecided() || doesLegacyHttpPolicyApply();
    }

    /** @return Whether a long-press gesture can resolve. */
    boolean canResolveLongpress() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.CONTEXTUAL_SEARCH_LONGPRESS_RESOLVE)
                || ChromeFeatureList.isEnabled(ChromeFeatureList.CONTEXTUAL_SEARCH_TRANSLATIONS);
    }

    /**
     * Returns whether surrounding context can be accessed by other systems or not.
     * @return Whether surroundings are available.
     */
    boolean canSendSurroundings() {
        if (mDidOverrideDecidedStateForTesting) return mDecidedStateForTesting;

        // The user must have decided on privacy to send page content on HTTPS.
        return !isUserUndecided() || doesLegacyHttpPolicyApply();
    }

    /**
     * @return Whether the Mandatory Promo is enabled.
     */
    boolean isMandatoryPromoAvailable() {
        if (!isUserUndecided()
                || !ContextualSearchFieldTrial.getSwitch(
                        ContextualSearchSwitch.IS_MANDATORY_PROMO_ENABLED)) {
            return false;
        }

        return getPromoOpenCount() >= ContextualSearchFieldTrial.getValue(
                       ContextualSearchSetting.MANDATORY_PROMO_LIMIT);
    }

    /**
     * @return Whether the Opt-out promo is available to be shown in any panel.
     */
    boolean isPromoAvailable() {
        return isUserUndecided();
    }

    /**
     * Registers that a tap has taken place by incrementing tap-tracking counters.
     */
    void registerTap() {
        if (isPromoAvailable()) {
            DisableablePromoTapCounter promoTapCounter = getPromoTapCounter();
            // Bump the counter only when it is still enabled.
            if (promoTapCounter.isEnabled()) promoTapCounter.increment();
        }
        int tapsSinceOpen = mPreferencesManager.incrementInt(
                ChromePreferenceKeys.CONTEXTUAL_SEARCH_TAP_SINCE_OPEN_COUNT);
        if (isUserUndecided()) {
            ContextualSearchUma.logTapsSinceOpenForUndecided(tapsSinceOpen);
        } else {
            ContextualSearchUma.logTapsSinceOpenForDecided(tapsSinceOpen);
        }
        mPreferencesManager.incrementInt(ChromePreferenceKeys.CONTEXTUAL_SEARCH_ALL_TIME_TAP_COUNT);
    }

    /**
     * Updates all the counters to account for an open-action on the panel.
     */
    void updateCountersForOpen() {
        // Always completely reset the tap counters that accumulate only since the last open.
        mPreferencesManager.writeInt(
                ChromePreferenceKeys.CONTEXTUAL_SEARCH_TAP_SINCE_OPEN_COUNT, 0);
        mPreferencesManager.writeInt(
                ChromePreferenceKeys.CONTEXTUAL_SEARCH_TAP_SINCE_OPEN_QUICK_ANSWER_COUNT, 0);

        // Disable the "promo tap" counter, but only if we're using the Opt-out onboarding.
        // For Opt-in, we never disable the promo tap counter.
        if (isPromoAvailable()) {
            getPromoTapCounter().disable();

            // Bump the total-promo-opens counter.
            int count = mPreferencesManager.incrementInt(
                    ChromePreferenceKeys.CONTEXTUAL_SEARCH_PROMO_OPEN_COUNT);
            ContextualSearchUma.logPromoOpenCount(count);
        }
        mPreferencesManager.incrementInt(
                ChromePreferenceKeys.CONTEXTUAL_SEARCH_ALL_TIME_OPEN_COUNT);
    }

    /**
     * Updates Tap counters to account for a quick-answer caption shown on the panel.
     * @param wasActivatedByTap Whether the triggering gesture was a Tap or not.
     * @param doesAnswer Whether the caption is considered an answer rather than just
     *                          informative.
     */
    void updateCountersForQuickAnswer(boolean wasActivatedByTap, boolean doesAnswer) {
        if (wasActivatedByTap && doesAnswer) {
            mPreferencesManager.incrementInt(
                    ChromePreferenceKeys.CONTEXTUAL_SEARCH_TAP_SINCE_OPEN_QUICK_ANSWER_COUNT);
            mPreferencesManager.incrementInt(
                    ChromePreferenceKeys.CONTEXTUAL_SEARCH_ALL_TIME_TAP_QUICK_ANSWER_COUNT);
        }
    }

    /**
     * @return Whether a verbatim request should be made for the given base page, assuming there
     *         is no existing request.
     */
    boolean shouldCreateVerbatimRequest() {
        @SelectionType
        int selectionType = mSelectionController.getSelectionType();
        return (mSelectionController.getSelectedText() != null
                && (selectionType == SelectionType.LONG_PRESS || !shouldPreviousGestureResolve()));
    }

    /** @return whether Tap is disabled due to the longpress experiment. */
    private boolean isTapDisabledDueToLongpress() {
        return canResolveLongpress()
                && !ContextualSearchFieldTrial.LONGPRESS_RESOLVE_PRESERVE_TAP.equals(
                        ChromeFeatureList.getFieldTrialParamByFeature(
                                ChromeFeatureList.CONTEXTUAL_SEARCH_LONGPRESS_RESOLVE,
                                ContextualSearchFieldTrial.LONGPRESS_RESOLVE_PARAM_NAME));
    }

    /**
     * Determines the policy for sending page content when on plain HTTP pages.
     * Checks a Feature to use our legacy HTTP policy instead of treating HTTP just like HTTPS.
     * See https://crbug.com/1129969 for details.
     * @return whether the legacy policy for plain HTTP pages currently applies.
     */
    private boolean doesLegacyHttpPolicyApply() {
        if (!isBasePageHTTP(mNetworkCommunicator.getBasePageUrl())) return false;

        // Check if the legacy behavior is enabled through a feature.
        return ChromeFeatureList.isEnabled(ChromeFeatureList.CONTEXTUAL_SEARCH_LEGACY_HTTP_POLICY);
    }

    /**
     * Determines whether an error from a search term resolution request should
     * be shown to the user, or not.
     */
    boolean shouldShowErrorCodeInBar() {
        // Builds with lots of real users should not see raw error codes.
        return !(ChromeVersionInfo.isStableBuild() || ChromeVersionInfo.isBetaBuild());
    }

    /**
     * Logs the current user's state, including preference, tap and open counters, etc.
     */
    void logCurrentState() {
        ContextualSearchUma.logPreferenceState();

        // Log the number of promo taps remaining.
        int promoTapsRemaining = getPromoTapsRemaining();
        if (promoTapsRemaining >= 0) ContextualSearchUma.logPromoTapsRemaining(promoTapsRemaining);

        // Also log the total number of taps before opening the promo, even for those
        // that are no longer tap limited. That way we'll know the distribution of the
        // number of taps needed before opening the promo.
        DisableablePromoTapCounter promoTapCounter = getPromoTapCounter();
        boolean wasOpened = !promoTapCounter.isEnabled();
        int count = promoTapCounter.getCount();
        if (wasOpened) {
            ContextualSearchUma.logPromoTapsBeforeFirstOpen(count);
        } else {
            ContextualSearchUma.logPromoTapsForNeverOpened(count);
        }
    }

    /**
     * Logs details about the Search Term Resolution.
     * Should only be called when a search term has been resolved.
     * @param searchTerm The Resolved Search Term.
     */
    void logSearchTermResolutionDetails(String searchTerm) {
        // Only log for decided users so the data reflect fully-enabled behavior.
        // Otherwise we'll get skewed data; more HTTP pages than HTTPS (since those don't resolve),
        // and it's also possible that public pages, e.g. news, have more searches for multi-word
        // entities like people.
        if (!isUserUndecided()) {
            URL url = mNetworkCommunicator.getBasePageUrl();
            ContextualSearchUma.logBasePageProtocol(isBasePageHTTP(url));
            boolean isSingleWord = !CONTAINS_WHITESPACE_PATTERN.matcher(searchTerm.trim()).find();
            ContextualSearchUma.logSearchTermResolvedWords(isSingleWord);
        }
    }

    /**
     * Whether sending the URL of the base page to the server may be done for policy reasons.
     * NOTE: There may be additional privacy reasons why the base page URL should not be sent.
     * TODO(donnd): Update this API to definitively determine if it's OK to send the URL,
     * by merging the checks in the native contextual_search_delegate here.
     * @return {@code true} if the URL may be sent for policy reasons.
     *         Note that a return value of {@code true} may still require additional checks
     *         to see if all privacy-related conditions are met to send the base page URL.
     */
    boolean maySendBasePageUrl() {
        return !isUserUndecided();
    }

    /**
     * The search provider icon is animated every time on long press if the user has never opened
     * the panel before and once a day on tap.
     *
     * @return Whether the search provider icon should be animated.
     */
    boolean shouldAnimateSearchProviderIcon() {
        if (mSearchPanel.isShowing()) return false;

        @SelectionType
        int selectionType = mSelectionController.getSelectionType();
        if (selectionType == SelectionType.TAP) {
            long currentTimeMillis = System.currentTimeMillis();
            long lastAnimatedTimeMillis = mPreferencesManager.readLong(
                    ChromePreferenceKeys.CONTEXTUAL_SEARCH_LAST_ANIMATION_TIME);
            if (Math.abs(currentTimeMillis - lastAnimatedTimeMillis) > DateUtils.DAY_IN_MILLIS) {
                mPreferencesManager.writeLong(
                        ChromePreferenceKeys.CONTEXTUAL_SEARCH_LAST_ANIMATION_TIME,
                        currentTimeMillis);
                return true;
            } else {
                return false;
            }
        } else if (selectionType == SelectionType.LONG_PRESS) {
            // If the panel has never been opened before, getPromoOpenCount() will be 0.
            // Once the panel has been opened, regardless of whether or not the user has opted-in or
            // opted-out, the promo open count will be greater than zero.
            return isUserUndecided() && getPromoOpenCount() == 0;
        }

        return false;
    }

    /**
     * @return Whether the given URL is used for Accelerated Mobile Pages by Google.
     */
    boolean isAmpUrl(String url) {
        Uri uri = Uri.parse(url);
        if (uri == null || uri.getHost() == null || uri.getPath() == null) return false;

        return uri.getHost().contains(DOMAIN_GOOGLE) && uri.getPath().startsWith(PATH_AMP);
    }

    // --------------------------------------------------------------------------------------------
    // Testing support.
    // --------------------------------------------------------------------------------------------

    /**
     * Overrides the decided/undecided state for the user preference.
     * @param decidedState Whether the user has decided or not.
     */
    @VisibleForTesting
    void overrideDecidedStateForTesting(boolean decidedState) {
        mDidOverrideDecidedStateForTesting = true;
        mDecidedStateForTesting = decidedState;
    }

    /**
     * @return count of times the panel with the promo has been opened.
     */
    @VisibleForTesting
    int getPromoOpenCount() {
        return mPreferencesManager.readInt(ChromePreferenceKeys.CONTEXTUAL_SEARCH_PROMO_OPEN_COUNT);
    }

    /**
     * @return The number of times the user has tapped since the last panel open.
     */
    @VisibleForTesting
    int getTapCount() {
        return mPreferencesManager.readInt(
                ChromePreferenceKeys.CONTEXTUAL_SEARCH_TAP_SINCE_OPEN_COUNT);
    }

    // --------------------------------------------------------------------------------------------
    // Translation support.
    // --------------------------------------------------------------------------------------------

    /**
     * Determines whether translation is needed between the given languages.
     * @param sourceLanguage The source language code; language we're translating from.
     * @param targetLanguages A list of target language codes; languages we might translate to.
     * @return Whether translation is needed or not.
     */
    boolean needsTranslation(String sourceLanguage, List<String> targetLanguages) {
        // For now, we just look for a language match.
        for (String targetLanguage : targetLanguages) {
            if (TextUtils.equals(sourceLanguage, targetLanguage)) {
                return false;
            }
        }
        return true;
    }

    /**
     * @return The best target language from the ordered list, or the empty string if
     *         none is available.
     */
    String bestTargetLanguage(List<String> targetLanguages) {
        return bestTargetLanguage(targetLanguages, Locale.getDefault().getCountry());
    }

    /**
     * Determines the best language to convert into, given the ordered list of languages the user
     * knows, and the UX language.
     * @param targetLanguages The list of languages to consider converting to.
     * @param countryOfUx The country of the UX.
     * @return the best language or an empty string.
     */
    @VisibleForTesting
    String bestTargetLanguage(List<String> targetLanguages, String countryOfUx) {
        // For now, we just return the first language, unless it's English
        // (due to over-usage).
        // TODO(donnd): Improve this logic. Determining the right language seems non-trivial.
        // E.g. If this language doesn't match the user's server preferences, they might see a page
        // in one language and the one box translation in another, which might be confusing.
        // Also this logic should only apply on Android, where English setup is overused.
        if (targetLanguages.size() > 1
                && TextUtils.equals(targetLanguages.get(0), Locale.ENGLISH.getLanguage())
                && !PREDOMINENTLY_ENGLISH_SPEAKING_COUNTRIES.contains(countryOfUx)) {
            return targetLanguages.get(1);
        } else if (targetLanguages.size() > 0) {
            return targetLanguages.get(0);
        } else {
            return "";
        }
    }

    /**
     * @return The ISO country code for the user's home country, or an empty string if not
     *         available or privacy-enabled.
     */
    @NonNull
    String getHomeCountry(Context context) {
        if (ContextualSearchFieldTrial.getSwitch(
                    ContextualSearchSwitch.IS_SEND_HOME_COUNTRY_DISABLED)) {
            return "";
        }

        TelephonyManager telephonyManager =
                (TelephonyManager) context.getSystemService(Context.TELEPHONY_SERVICE);
        if (telephonyManager == null) return "";

        String simCountryIso = telephonyManager.getSimCountryIso();
        return TextUtils.isEmpty(simCountryIso) ? "" : simCountryIso;
    }

    /**
     * @return Whether a promo is needed because the user is still undecided
     *         on enabling or disabling the feature.
     */
    boolean isUserUndecided() {
        if (mDidOverrideDecidedStateForTesting) return !mDecidedStateForTesting;

        return ContextualSearchManager.isContextualSearchUninitialized();
    }

    /**
     * @param url The URL of the base page.
     * @return Whether the given content view is for an HTTP page.
     */
    boolean isBasePageHTTP(@Nullable URL url) {
        return url != null && UrlConstants.HTTP_SCHEME.equals(url.getProtocol());
    }

    // --------------------------------------------------------------------------------------------
    // Related Searches Support.
    // --------------------------------------------------------------------------------------------

    /**
     * @return Whether the experimental Feature for Related Searches is enabled.
     */
    boolean isRelatedSearchesEnabled() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.RELATED_SEARCHES);
    }

    /**
     * @return Whether we're currently processing a Related Search gesture.
     */
    boolean isProcessingRelatedSearch() {
        return isRelatedSearchesEnabled()
                && mSelectionController.getSelectionType() == SelectionType.TAP;
    }

    /**
     * @return The number of times a navigation in the panel can be done without promoting the
     *         panel into a separate tab.
     */
    int navigateWithoutPromotionLimitForRelatedSearches() {
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.RELATED_SEARCHES)
                && mSelectionController.getSelectionType() == SelectionType.TAP) {
            // For Related Searches the returned page has a list of search-result pages
            // that the user can choose from, so initial navigation should be done
            // without promotion.  We want normal behavior for Longpress.
            return 2;
        }
        return 1;
    }

    /**
     * Overrides the selection if we're processing a Related Searches gesture.
    * @param selection The original selection.  This is returned if not processing Related
             Searches.
    * @param relatedSearchesWord The word to show if we are processing Related Searches.
    * @return the input or an override of the selection appropriate for experiments.
    */
    String overrideSelectionIfProcessingRelatedSearches(
            String selection, String relatedSearchesWord) {
        return isProcessingRelatedSearch() ? relatedSearchesWord : selection;
    }

    /** @return whether doing Related Searches should be part of processing the current request. */
    boolean doRelatedSearches() {
        // TODO(donnd): Update this along with crbug.com/1119585.
        return isProcessingRelatedSearch();
    }

    // --------------------------------------------------------------------------------------------
    // Testing helpers.
    // --------------------------------------------------------------------------------------------

    /**
     * Sets the {@link ContextualSearchNetworkCommunicator} to use for server requests.
     * @param networkCommunicator The communicator for all future requests.
     */
    @VisibleForTesting
    public void setNetworkCommunicator(ContextualSearchNetworkCommunicator networkCommunicator) {
        mNetworkCommunicator = networkCommunicator;
    }
}
