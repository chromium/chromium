// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import android.content.Context;
import android.net.Uri;
import android.telephony.TelephonyManager;
import android.text.TextUtils;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.compositor.bottombar.contextualsearch.ContextualSearchPanelInterface;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchInternalStateController.InternalState;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchSelectionController.SelectionType;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchUma.ContextualSearchPreference;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.prefetch.settings.PreloadPagesSettingsBridge;
import org.chromium.chrome.browser.prefetch.settings.PreloadPagesState;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.signin.services.UnifiedConsentServiceBridge;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.version_info.VersionInfo;
import org.chromium.url.GURL;

/**
 * Handles business decision policy for the {@code ContextualSearchManager}.
 */
class ContextualSearchPolicy {
    private static final String TAG = "ContextualSearch";
    private static final String DOMAIN_GOOGLE = "google";
    private static final String PATH_AMP = "/amp/";
    private static final int REMAINING_NOT_APPLICABLE = -1;
    private static final int TAP_TRIGGERED_PROMO_LIMIT = 50;
    private static final int PROMO_DEFAULT_LIMIT = 3;

    // Constants related to the Contextual Search preference.
    private static final String CONTEXTUAL_SEARCH_DISABLED = "false";
    private static final String CONTEXTUAL_SEARCH_ENABLED = "true";

    private final SharedPreferencesManager mPreferencesManager;
    private final ContextualSearchSelectionController mSelectionController;
    private final RelatedSearchesStamp mRelatedSearchesStamp;
    private ContextualSearchNetworkCommunicator mNetworkCommunicator;
    private ContextualSearchPanelInterface mSearchPanel;

    // Members used only for testing purposes.
    private boolean mDidOverrideFullyEnabledForTesting;
    private boolean mFullyEnabledForTesting;
    private Integer mTapTriggeredPromoLimitForTesting;
    private boolean mDidOverrideAllowSendingPageUrlForTesting;
    private boolean mAllowSendingPageUrlForTesting;

    /**
     * ContextualSearchPolicy constructor.
     */
    public ContextualSearchPolicy(ContextualSearchSelectionController selectionController,
            ContextualSearchNetworkCommunicator networkCommunicator) {
        mPreferencesManager = ChromeSharedPreferences.getInstance();

        mSelectionController = selectionController;
        mNetworkCommunicator = networkCommunicator;
        if (selectionController != null) selectionController.setPolicy(this);
        mRelatedSearchesStamp = new RelatedSearchesStamp(this);
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
        return isContextualSearchFullyEnabled() ? true : (getPromoTapsRemaining() != 0);
    }

    /**
     * @return whether or not the Contextual Search Result should be preloaded before the user
     *         explicitly interacts with the feature.
     */
    boolean shouldPrefetchSearchResult() {
        if (PreloadPagesSettingsBridge.getState() == PreloadPagesState.NO_PRELOADING) {
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
        return mSelectionController.getSelectionType() == SelectionType.TAP
                || mSelectionController.getSelectionType() == SelectionType.RESOLVING_LONG_PRESS;
    }

    /**
     * Determines whether the gesture being processed is allowed to resolve.
     * TODO(donnd): rename to be more descriptive. Maybe isGestureAllowedToResolve?
     * @return Whether the previous gesture should resolve.
     */
    boolean shouldPreviousGestureResolve() {
        // The user must have decided on privacy to resolve page content on HTTPS.
        return isContextualSearchFullyEnabled();
    }

    /**
     * Returns whether surrounding context can be accessed by other systems or not.
     * @return Whether surroundings are available.
     */
    boolean canSendSurroundings() {
        // The user must have decided on privacy to send page content on HTTPS.
        return isContextualSearchFullyEnabled();
    }

    /**
     * @return Whether the Opt-out promo is available to be shown in any panel.
     */
    boolean isPromoAvailable() {
        // Only show promo card a limited number of times.
        return isUserUndecided() && getContextualSearchPromoCardShownCount() < PROMO_DEFAULT_LIMIT;
    }

    /**
     * Returns whether conditions are right for an IPH for Longpress to be shown.
     * We only show this for users that have already opted-in because it's all about using page
     * context with the right gesture.
     */
    boolean isLongpressInPanelHelpCondition() {
        // We no longer support an IPH in the panel for promoting a Longpress instead of a Tap.
        return false;
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
    }

    /**
     * Updates all the counters to account for an open-action on the panel.
     */
    void updateCountersForOpen() {
        // Disable the "promo tap" counter, but only if we're using the Opt-out onboarding.
        // For Opt-in, we never disable the promo tap counter.
        if (isPromoAvailable()) {
            getPromoTapCounter().disable();
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
     * Determines whether an error from a search term resolution request should
     * be shown to the user, or not.
     */
    boolean shouldShowErrorCodeInBar() {
        // Builds with lots of real users should not see raw error codes.
        return !(VersionInfo.isStableBuild() || VersionInfo.isBetaBuild());
    }

    /**
     * Logs the current user's state, including preference, tap and open counters, etc.
     */
    void logCurrentState() {
        ContextualSearchUma.logPreferenceState();
        RelatedSearchesUma.logRelatedSearchesPermissionsForAllUsers(
                hasSendUrlPermissions(), canSendSurroundings());
    }

    /**
     * Logs whether the current user is qualified to do Related Searches requests. This does not
     * check if Related Searches is actually enabled for the current user, only whether they are
     * qualified. We use this to gauge whether each group has a balanced number of qualified users.
     * Can be logged multiple times since we'll just look at the user-count of this histogram.
     * @param basePageLanguage The language of the page, to check if supported by the server.
     */
    void logRelatedSearchesQualifiedUsers(String basePageLanguage) {
        if (mRelatedSearchesStamp.isQualifiedForRelatedSearches(basePageLanguage)) {
            RelatedSearchesUma.logRelatedSearchesQualifiedUsers();
        }
    }

    /**
     * Whether this request should include sending the URL of the base page to the server.
     * Several conditions are checked to make sure it's OK to send the URL, but primarily this is
     * based on whether the user has checked the setting for "Make searches and browsing better".
     * @return {@code true} if the URL should be sent.
     */
    boolean doSendBasePageUrl() {
        if (!isContextualSearchFullyEnabled()) return false;

        // Ensure that the default search provider is Google.
        if (!TemplateUrlServiceFactory.getForProfile(Profile.getLastUsedRegularProfile())
                        .isDefaultSearchEngineGoogle()) {
            return false;
        }

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
    boolean hasSendUrlPermissions() {
        if (mDidOverrideAllowSendingPageUrlForTesting) return mAllowSendingPageUrlForTesting;

        // Check whether the user has enabled anonymous URL-keyed data collection.
        // This is surfaced on the relatively new "Make searches and browsing better" user setting.
        // In case an experiment is active for the legacy UI call through the unified consent
        // service.
        return UnifiedConsentServiceBridge.isUrlKeyedAnonymizedDataCollectionEnabled(
                Profile.getLastUsedRegularProfile());
    }

    /**
     * Returns whether a transition that is both from and to the given state should be done.
     * This allows prevention of the short-circuiting that ignores a state transition to the current
     * state in cases where rerunning the current state might safeguard against problematic
     * behavior.
     * @param state The current state, which is also the state being transitioned into.
     * @return {@code true} to go ahead with the logic for that state transition even though we're
     *     already in that state. {@code false} indicates that ignoring this redundant state
     *     transition is fine.
     */
    boolean shouldRetryCurrentState(@InternalState int state) {
        // Make sure we don't get stuck in the IDLE state if the panel is still showing.
        // See https://crbug.com/1251774
        return state == InternalState.IDLE && mSearchPanel != null
                && (mSearchPanel.isShowing() || mSearchPanel.isActive());
    }

    /**
     * @return Whether the given URL is used for Accelerated Mobile Pages by Google.
     */
    boolean isAmpUrl(String url) {
        Uri uri = Uri.parse(url);
        if (uri == null || uri.getHost() == null || uri.getPath() == null) return false;

        return uri.getHost().contains(DOMAIN_GOOGLE) && uri.getPath().startsWith(PATH_AMP);
    }

    /**
     * @return Whether the Contextual Search feature was disabled by the user explicitly.
     */
    static boolean isContextualSearchDisabled() {
        return getPrefService()
                .getString(Pref.CONTEXTUAL_SEARCH_ENABLED)
                .equals(CONTEXTUAL_SEARCH_DISABLED);
    }

    /**
     * @return Whether the Contextual Search feature was enabled by the user explicitly.
     */
    static boolean isContextualSearchEnabled() {
        return getPrefService()
                .getString(Pref.CONTEXTUAL_SEARCH_ENABLED)
                .equals(CONTEXTUAL_SEARCH_ENABLED);
    }

    /**
     * @return Whether the Contextual Search feature is uninitialized (preference unset by the
     *         user).
     */
    static boolean isContextualSearchUninitialized() {
        return getPrefService().getString(Pref.CONTEXTUAL_SEARCH_ENABLED).isEmpty();
    }

    /**
     * @return Whether the Contextual Search fully privacy opt-in was disabled by the user
     *         explicitly.
     */
    static boolean isContextualSearchOptInDisabled() {
        return !getPrefService().getBoolean(Pref.CONTEXTUAL_SEARCH_WAS_FULLY_PRIVACY_ENABLED);
    }

    /**
     * @return Whether the Contextual Search fully privacy opt-in was enabled by the user
     *         explicitly.
     */
    static boolean isContextualSearchOptInEnabled() {
        return getPrefService().getBoolean(Pref.CONTEXTUAL_SEARCH_WAS_FULLY_PRIVACY_ENABLED);
    }

    /**
     * @return Whether the Contextual Search fully privacy opt-in is uninitialized (preference unset
     *         by the user).
     */
    static boolean isContextualSearchOptInUninitialized() {
        return !getPrefService().hasPrefPath(Pref.CONTEXTUAL_SEARCH_WAS_FULLY_PRIVACY_ENABLED);
    }

    /**
     * @return Count of times the promo card has been shown.
     */
    static int getContextualSearchPromoCardShownCount() {
        return getPrefService().getInteger(Pref.CONTEXTUAL_SEARCH_PROMO_CARD_SHOWN_COUNT);
    }

    /**
     * Sets Count of times the promo card has been shown.
     */
    private static void setContextualSearchPromoCardShownCount(int count) {
        getPrefService().setInteger(Pref.CONTEXTUAL_SEARCH_PROMO_CARD_SHOWN_COUNT, count);
    }

    /**
     * @return Whether the Contextual Search feature is disabled when the prefs service considers it
     *         managed.
     */
    static boolean isContextualSearchDisabledByPolicy() {
        return getPrefService().isManagedPreference(Pref.CONTEXTUAL_SEARCH_ENABLED)
                && isContextualSearchDisabled();
    }

    /**
     * Explicitly set whether Contextual Search is enabled or not, with the enabled state being
     * either fully or default-enabled based on previous state. 'enabled' is true - fully opt in or
     * default-enabled based on previous state. 'enabled' is false - the feature is disabled.
     * @param enabled Whether Contextual Search should be enabled.
     */
    static void setContextualSearchState(boolean enabled) {
        @ContextualSearchPreference
        int onState = isContextualSearchOptInEnabled() ? ContextualSearchPreference.ENABLED
                                                       : ContextualSearchPreference.UNINITIALIZED;
        setContextualSearchStateInternal(enabled ? onState : ContextualSearchPreference.DISABLED);
    }

    /**
     * @return Whether the Contextual Search feature was fully opted in based on the preference
     *         itself.
     */
    static boolean isContextualSearchPrefFullyOptedIn() {
        return isContextualSearchOptInUninitialized() ? isContextualSearchEnabled()
                                                      : isContextualSearchOptInEnabled();
    }

    /**
     * Sets whether the user is fully opted in for Contextual Search Privacy.
     * 'enabled' is true - fully opt in.
     * 'enabled' is false - remain undecided.
     * @param enabled Whether Contextual Search privacy is opted in.
     */
    static void setContextualSearchFullyOptedIn(boolean enabled) {
        getPrefService().setBoolean(Pref.CONTEXTUAL_SEARCH_WAS_FULLY_PRIVACY_ENABLED, enabled);
        setContextualSearchStateInternal(enabled ? ContextualSearchPreference.ENABLED
                                                 : ContextualSearchPreference.UNINITIALIZED);
    }

    /** Notifies that a promo card has been shown. */
    static void onPromoShown() {
        int count = getContextualSearchPromoCardShownCount();
        count++;
        setContextualSearchPromoCardShownCount(count);
        ContextualSearchUma.logRevisedPromoOpenCount(count);
    }

    /**
     * @param state The state for the Contextual Search.
     */
    private static void setContextualSearchStateInternal(@ContextualSearchPreference int state) {
        switch (state) {
            case ContextualSearchPreference.UNINITIALIZED:
                getPrefService().clearPref(Pref.CONTEXTUAL_SEARCH_ENABLED);
                break;
            case ContextualSearchPreference.ENABLED:
                getPrefService().setString(
                        Pref.CONTEXTUAL_SEARCH_ENABLED, CONTEXTUAL_SEARCH_ENABLED);
                break;
            case ContextualSearchPreference.DISABLED:
                getPrefService().setString(
                        Pref.CONTEXTUAL_SEARCH_ENABLED, CONTEXTUAL_SEARCH_DISABLED);
                break;
            default:
                Log.e(TAG, "Unexpected state for ContextualSearchPreference state=" + state);
                break;
        }
    }

    /**
     * @return The PrefService associated with last used Profile.
     */
    private static PrefService getPrefService() {
        return UserPrefs.get(Profile.getLastUsedRegularProfile());
    }

    // --------------------------------------------------------------------------------------------
    // Testing support.
    // --------------------------------------------------------------------------------------------

    /**
     * Overrides the decided/undecided state for the user preference.
     * @param decidedState Whether the user has decided to opt-in to sending page content or not.
     * @return whether the previous decided state was fully enabled or not.
     */
    boolean overrideDecidedStateForTesting(boolean decidedState) {
        boolean wasEnabled = mFullyEnabledForTesting;
        mDidOverrideFullyEnabledForTesting = true;
        mFullyEnabledForTesting = decidedState;
        return wasEnabled;
    }

    /**
     * Overrides the user preference for sending the page URL to Google.
     * @param doAllowSendingPageUrl Whether to allow sending the page URL or not, for tests.
     */
    void overrideAllowSendingPageUrlForTesting(boolean doAllowSendingPageUrl) {
        mDidOverrideAllowSendingPageUrlForTesting = true;
        mAllowSendingPageUrlForTesting = doAllowSendingPageUrl;
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
        if (mDidOverrideFullyEnabledForTesting) return !mFullyEnabledForTesting;

        return isContextualSearchUninitialized() && isContextualSearchOptInUninitialized();
    }

    /**
     * @return Whether a user explicitly enabled the Contextual Search feature.
     */
    boolean isContextualSearchFullyEnabled() {
        if (mDidOverrideFullyEnabledForTesting) return mFullyEnabledForTesting;

        return isContextualSearchEnabled();
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
        return mRelatedSearchesStamp.getRelatedSearchesStamp(basePageLanguage);
    }

    /**
     * @return whether the given parameter is currently enabled in the Related Searches Variation
     *         configuration.
     */
    boolean isRelatedSearchesParamEnabled(String paramName) {
        return ContextualSearchFieldTrial.isRelatedSearchesParamEnabled(paramName);
    }

    /** @return whether we're missing the Related Searches configuration stamp. */
    boolean isMissingRelatedSearchesConfiguration() {
        return TextUtils.isEmpty(
                ContextualSearchFieldTrial.getRelatedSearchesExperimentConfigurationStamp());
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
