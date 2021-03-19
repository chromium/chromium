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
import org.chromium.chrome.browser.compositor.bottombar.contextualsearch.ContextualSearchPanelInterface;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchFieldTrial.ContextualSearchSetting;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchFieldTrial.ContextualSearchSwitch;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchSelectionController.SelectionType;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.privacy.settings.PrivacyPreferencesManagerImpl;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.signin.services.UnifiedConsentServiceBridge;
import org.chromium.chrome.browser.version.ChromeVersionInfo;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.url.GURL;

import java.util.HashSet;
import java.util.regex.Pattern;

/**
 * Handles business decision policy for the {@code ContextualSearchManager}.
 */
class ContextualSearchPolicy {
    private static final Pattern CONTAINS_WHITESPACE_PATTERN = Pattern.compile("\\s");
    private static final String DOMAIN_GOOGLE = "google";
    private static final String PATH_AMP = "/amp/";
    private static final int REMAINING_NOT_APPLICABLE = -1;
    private static final int TAP_TRIGGERED_PROMO_LIMIT = 50;
    // Related Searches "stamp" building and accessing details.
    private static final String RELATED_SEARCHES_STAMP_VERSION = "1";
    private static final String RELATED_SEARCHES_EXPERIMENT_RECIPE_STAGE = "R";
    private static final String RELATED_SEARCHES_NO_EXPERIMENT = "n";
    private static final String RELATED_SEARCHES_LANGUAGE_RESTRICTION = "l";
    private static final String RELATED_SEARCHES_DARK_LAUNCH = "d";
    private static final String NO_EXPERIMENT_STAMP = RELATED_SEARCHES_STAMP_VERSION
            + RELATED_SEARCHES_EXPERIMENT_RECIPE_STAGE + RELATED_SEARCHES_NO_EXPERIMENT;
    /**
     * Verbosity param used to control requested results.
     * <ul>
     *   <li> "d" specifies a dark launch, which means return none </li>
     *   <li> "v" for verbose  </li>
     *   <li> "x" for extra verbose </li>
     *   <li> "" for the default </li></ul>
     * See also the verbosity entry in about_flags to correlate.
     */
    private static final String RELATED_SEARCHES_VERBOSITY_PARAM = "verbosity";

    // TODO(donnd): remove -- deprecated.
    private static final HashSet<String> PREDOMINENTLY_ENGLISH_SPEAKING_COUNTRIES =
            CollectionUtil.newHashSet("GB", "US");

    private final SharedPreferencesManager mPreferencesManager;
    private final ContextualSearchSelectionController mSelectionController;
    private ContextualSearchNetworkCommunicator mNetworkCommunicator;
    private ContextualSearchPanelInterface mSearchPanel;

    // Members used only for testing purposes.
    private boolean mDidOverrideDecidedStateForTesting;
    private boolean mDecidedStateForTesting;
    private Integer mTapTriggeredPromoLimitForTesting;
    private boolean mDidOverrideAllowSendingPageUrlForTesting;
    private boolean mAllowSendingPageUrlForTesting;

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
    public void setContextualSearchPanel(ContextualSearchPanelInterface panel) {
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
                || !PrivacyPreferencesManagerImpl.getInstance().getNetworkPredictionEnabled()) {
            return false;
        }

        // We never preload unless we have sent page context (done through a Resolve request).
        // Only some gestures can resolve, and only when resolve privacy rules are met.
        return isResolvingGesture() && shouldPreviousGestureResolve();
    }

    /**
     * Determines whether the current gesture can trigger a resolve request to use page context.
     * This only checks the gesture, not privacy status -- {@see #shouldPreviousGestureResolve}.
     */
    boolean isResolvingGesture() {
        return (mSelectionController.getSelectionType() == SelectionType.TAP
                       && !isLiteralSearchTapEnabled())
                || mSelectionController.getSelectionType() == SelectionType.RESOLVING_LONG_PRESS;
    }

    /**
     * Determines whether the gesture being processed is allowed to resolve.
     * TODO(donnd): rename to be more descriptive. Maybe isGestureAllowedToResolve?
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
     * Returns whether conditions are right for an IPH for Longpress to be shown.
     * We only show this for users that have already opted-in because it's all about using page
     * context with the right gesture.
     */
    boolean isLongpressInPanelHelpCondition() {
        return mSelectionController.isTapSelection() && canResolveLongpress() && !isUserUndecided();
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

    /**
     * @return whether the experiment that causes a tap gesture to trigger a literal search for the
     *         selection (rather than sending context to resolve a search term) is enabled.
     */
    boolean isLiteralSearchTapEnabled() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.CONTEXTUAL_SEARCH_LITERAL_SEARCH_TAP);
    }

    /** @return whether Tap is disabled due to the longpress experiment. */
    private boolean isTapDisabledDueToLongpress() {
        return canResolveLongpress() && !isLiteralSearchTapEnabled();
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
        ContextualSearchUma.logRelatedSearchesPermissionsForAllUsers(
                hasSendUrlPermissions(), canSendSurroundings());

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
            GURL url = mNetworkCommunicator.getBasePageUrl();
            ContextualSearchUma.logBasePageProtocol(isBasePageHTTP(url));
            boolean isSingleWord = !CONTAINS_WHITESPACE_PATTERN.matcher(searchTerm.trim()).find();
            ContextualSearchUma.logSearchTermResolvedWords(isSingleWord);
        }
    }

    /**
     * Logs whether the current user is qualified to do Related Searches requests. This does not
     * check if Related Searches is actually enabled for the current user, only whether they are
     * qualified. We use this to gauge whether each group has a balanced number of qualified users.
     * Can be logged multiple times since we'll just look at the user-count of this histogram.
     * @param basePageLanguage The language of the page, to check if supported by the server.
     */
    void logRelatedSearchesQualifiedUsers(String basePageLanguage) {
        if (isQualifiedForRelatedSearches(basePageLanguage)) {
            ContextualSearchUma.logRelatedSearchesQualifiedUsers();
        }
    }

    /**
     * Whether this request should include sending the URL of the base page to the server.
     * Several conditions are checked to make sure it's OK to send the URL, but primarily this is
     * based on whether the user has checked the setting for "Make searches and browsing better".
     * @return {@code true} if the URL should be sent.
     */
    boolean doSendBasePageUrl() {
        if (isUserUndecided()) return false;

        // Check whether there is a Field Trial setting preventing us from sending the page URL.
        if (ContextualSearchFieldTrial.getSwitch(
                    ContextualSearchSwitch.IS_SEND_BASE_PAGE_URL_DISABLED)) {
            return false;
        }

        // Ensure that the default search provider is Google.
        if (!TemplateUrlServiceFactory.get().isDefaultSearchEngineGoogle()) return false;

        // Only allow HTTP or HTTPS URLs.
        GURL url = mNetworkCommunicator.getBasePageUrl();

        if (url == null || !UrlUtilities.isHttpOrHttps(url)) {
            return false;
        }

        return hasSendUrlPermissions();
    }

    /**
     * Determines whether the user has given permission to send URLs through the "Make searches and
     * browsing better" user setting.
     * @return Whether we can send a URL.
     */
    private boolean hasSendUrlPermissions() {
        if (mDidOverrideAllowSendingPageUrlForTesting) return mAllowSendingPageUrlForTesting;

        // Check whether the user has enabled anonymous URL-keyed data collection.
        // This is surfaced on the relatively new "Make searches and browsing better" user setting.
        // In case an experiment is active for the legacy UI call through the unified consent
        // service.
        return UnifiedConsentServiceBridge.isUrlKeyedAnonymizedDataCollectionEnabled(
                Profile.getLastUsedRegularProfile());
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
     * Overrides the user preference for sending the page URL to Google.
     * @param doAllowSendingPageUrl Whether to allow sending the page URL or not, for tests.
     */
    @VisibleForTesting
    void overrideAllowSendingPageUrlForTesting(boolean doAllowSendingPageUrl) {
        mDidOverrideAllowSendingPageUrlForTesting = true;
        mAllowSendingPageUrlForTesting = doAllowSendingPageUrl;
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
    // Additional considerations.
    // --------------------------------------------------------------------------------------------

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
    boolean isBasePageHTTP(@Nullable GURL url) {
        return url != null && UrlConstants.HTTP_SCHEME.equals(url.getScheme());
    }

    // --------------------------------------------------------------------------------------------
    // Related Searches Support.
    // --------------------------------------------------------------------------------------------

    /**
     * Gets the runtime processing stamp for Related Searches. This typically gets the value from
     * a param from a Field Trial Feature.
     * @param basePageLanguage The language of the page, to check for server support.
     * @return A {@code String} whose value describes the schema version and current processing
     *         of Related Searches, or an empty string if the user is not qualified to request
     *         Related Searches or the feature is not enabled.
     */
    String getRelatedSearchesStamp(String basePageLanguage) {
        if (!isRelatedSearchesQualifiedAndEnabled(basePageLanguage)) return "";

        boolean isLanguageRestricted =
                !TextUtils.isEmpty(ContextualSearchFieldTrial.getRelatedSearchesParam(
                        ContextualSearchFieldTrial.RELATED_SEARCHES_LANGUAGE_ALLOWLIST_PARAM_NAME));
        return buildRelatedSearchesStamp(isLanguageRestricted);
    }

    /**
     * Checks if the current user is both qualified to do Related Searches and has the feature
     * enabled. Qualifications may include restrictions on language during early development.
     * @param basePageLanguage The language of the page, to check for server support.
     * @return Whether the user is qualified to get Related Searches suggestions and the
     *         experimental feature is enabled.
     */
    boolean isRelatedSearchesQualifiedAndEnabled(String basePageLanguage) {
        return isQualifiedForRelatedSearches(basePageLanguage)
                && ChromeFeatureList.isEnabled(ChromeFeatureList.RELATED_SEARCHES);
    }

    /**
     * Determines if the current user is qualified for Related Searches. There may be language
     * and privacy restrictions on whether users can activate Related Searches, and some of these
     * requirements are determined at runtime based on Variations params.
     * @param basePageLanguage The language of the page, to check for server support.
     * @return Whether the user could do a Related Searches request if Feature-enabled.
     */
    private boolean isQualifiedForRelatedSearches(String basePageLanguage) {
        return isLanguageQualified(basePageLanguage) && canSendUrlIfNeeded()
                && canSendContentIfNeeded();
    }

    /**
     * Checks if the language of the page qualifies for Related Searches.
     * We check the Variations config for a parameter that lists allowed languages so we can know
     * what the server currently supports. If there's no allow list then any language will work.
     * @param basePageLanguage The language of the page, to check for server support.
     * @return whether the supplied parameter satisfies the current language requirement.
     */
    private boolean isLanguageQualified(String basePageLanguage) {
        String allowedLanguages = ContextualSearchFieldTrial.getRelatedSearchesParam(
                ContextualSearchFieldTrial.RELATED_SEARCHES_LANGUAGE_ALLOWLIST_PARAM_NAME);
        return TextUtils.isEmpty(allowedLanguages) || allowedLanguages.contains(basePageLanguage);
    }

    /**
     * @return whether the user's privacy setting for URL sending satisfies the configured
     *         requirement.
     */
    private boolean canSendUrlIfNeeded() {
        return !isRelatedSearchesUrlNeeded() || hasSendUrlPermissions();
    }

    /**
     * @return whether the user's privacy setting for page content sending satisfies the configured
     *         requirement.
     */
    private boolean canSendContentIfNeeded() {
        return !isRelatedSearchesContentNeeded() || !isUserUndecided();
    }

    /** @return whether the runtime configuration has a URL sending permissions requirement. */
    private boolean isRelatedSearchesUrlNeeded() {
        return isRelatedSearchesParamEnabled(
                       ContextualSearchFieldTrial.RELATED_SEARCHES_NEEDS_URL_PARAM_NAME)
                || isMissingRelatedSearchesConfiguration();
    }

    /**
     * @return whether the runtime configuration has a page content sending permissions
     *         requirement.
     */
    private boolean isRelatedSearchesContentNeeded() {
        return isRelatedSearchesParamEnabled(
                       ContextualSearchFieldTrial.RELATED_SEARCHES_NEEDS_CONTENT_PARAM_NAME)
                || isMissingRelatedSearchesConfiguration();
    }

    /**
     * @return whether the given parameter is currently enabled in the Related Searches Variation
     *         configuration.
     */
    private boolean isRelatedSearchesParamEnabled(String paramName) {
        return ContextualSearchFieldTrial.isRelatedSearchesParamEnabled(paramName);
    }

    /** @return whether we're missing the Related Searches configuration stamp. */
    private boolean isMissingRelatedSearchesConfiguration() {
        return TextUtils.isEmpty(
                ContextualSearchFieldTrial.getRelatedSearchesExperiementConfigurationStamp());
    }

    /**
     * Builds the "stamp" that tracks the processing of Related Searches and describes what was
     * done at each stage using a shorthand notation. The notation is described in go/rsearches-dd
     * here: http://doc/1DryD8NAP5LQAo326LnxbqkIDCNfiCOB7ak3gAYaNWAM#bookmark=id.nx7ivu2upqw
     * <p>The first stage is built here: "1" for schema version one, "R" for the configuration
     * Recipe which has a character describing how we'll formulate the search. Typically all of
     * this comes from the Variations config at runtime. We programmatically append an "l" that
     * indicates a language restriction (when present), and currently a "d" for "dark launch" so
     * the server knows to return normal Contextual Search results for this older client.
     * @param isLanguageRestricted Whether there are any language restrictions needed by the
     *        server.
     * @return A string that represents and encoded description of the current request processing.
     */
    private String buildRelatedSearchesStamp(boolean isLanguageRestricted) {
        String experimentConfigStamp =
                ContextualSearchFieldTrial.getRelatedSearchesExperiementConfigurationStamp();
        if (TextUtils.isEmpty(experimentConfigStamp)) experimentConfigStamp = NO_EXPERIMENT_STAMP;
        StringBuilder stampBuilder = new StringBuilder().append(experimentConfigStamp);
        if (isLanguageRestricted) stampBuilder.append(RELATED_SEARCHES_LANGUAGE_RESTRICTION);
        // Add a tag so the server knows this version of the client is doing a dark launch
        // and cannot decode Related Searches, unless overridden by a Feature flag.
        String resultsToReturnCode = getNumberOfRelatedSearchesToRequestCode();
        if (resultsToReturnCode.length() > 0) stampBuilder.append(resultsToReturnCode);
        return stampBuilder.toString();
    }

    /**
     * Returns the number of results to request from the server, as a single coded letter, or
     * {@code null} if the server should just return the default number of Related Searches.
     */
    private String getNumberOfRelatedSearchesToRequestCode() {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.RELATED_SEARCHES_UI)) {
            return RELATED_SEARCHES_DARK_LAUNCH;
        }
        // Return the Feature param, which could be an empty string if not present.
        return ChromeFeatureList.getFieldTrialParamByFeature(
                ChromeFeatureList.RELATED_SEARCHES_UI, RELATED_SEARCHES_VERBOSITY_PARAM);
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
