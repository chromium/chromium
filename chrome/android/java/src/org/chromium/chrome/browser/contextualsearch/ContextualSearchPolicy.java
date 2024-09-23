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

import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.Log;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.version_info.VersionInfo;
import org.chromium.chrome.browser.compositor.bottombar.contextualsearch.ContextualSearchPanel;
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
import org.chromium.url.GURL;

/** Handles business decision policy for the {@code ContextualSearchManager}. */
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
    private final Profile mProfile;
    private final ContextualSearchSelectionController mSelectionController;
    private final RelatedSearchesStamp mRelatedSearchesStamp;
    private ContextualSearchNetworkCommunicator mNetworkCommunicator;
    private ContextualSearchPanel mSearchPanel;

    // Members used only for testing purposes.
    private boolean mDidOverrideFullyEnabledForTesting;
    private boolean mFullyEnabledForTesting;
    private Integer mTapTriggeredPromoLimitForTesting;
    private boolean mDidOverrideAllowSendingPageUrlForTesting;
    private boolean mAllowSendingPageUrlForTesting;
    private Boolean mContextualSearchResolutionUrlValid;

    /** ContextualSearchPolicy constructor. */
    public ContextualSearchPolicy(
            Profile profile,
            ContextualSearchSelectionController selectionController,
            ContextualSearchNetworkCommunicator networkCommunicator) {
        mPreferencesManager = ChromeSharedPreferences.getInstance();

        mProfile = profile;
        mSelectionController = selectionController;
        mNetworkCommunicator = networkCommunicator;
        mRelatedSearchesStamp = new RelatedSearchesStamp(this);
    }

    /**
     * Sets the handle to the ContextualSearchPanel.
     *
     * @param panel The ContextualSearchPanel.
     */
    public void setContextualSearchPanel(ContextualSearchPanel panel) {
        mSearchPanel = panel;
    }

    /**
     * @return The number of additional times to show the promo on tap, 0 if it should not be shown,
     *     or a negative value if the counter has been disabled or the user has accepted the promo.
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
     *     explicitly interacts with the feature.
     */
    boolean shouldPrefetchSearchResult() {
        if (PreloadPagesSettingsBridge.getState(mProfile) == PreloadPagesState.NO_PRELOADING) {
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
        return isUserUndecided()
                && getContextualSearchPromoCardShownCount(mProfile) < PROMO_DEFAULT_LIMIT;
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

    /** Registers that a tap has taken place by incrementing tap-tracking counters. */
    void registerTap() {
        if (isPromoAvailable()) {
            DisableablePromoTapCounter promoTapCounter = getPromoTapCounter();
            // Bump the counter only when it is still enabled.
            if (promoTapCounter.isEnabled()) promoTapCounter.increment();
        }
    }

    /** Updates all the counters to account for an open-action on the panel. */
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
        @SelectionType int selectionType = mSelectionController.getSelectionType();
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

    /** Logs the current user's state, including preference, tap and open counters, etc. */
    void logCurrentState() {
        ContextualSearchUma.logPreferenceState(mProfile);
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
        if (!TemplateUrlServiceFactory.getForProfile(mProfile).isDefaultSearchEngineGoogle()) {
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
        return UnifiedConsentServiceBridge.isUrlKeyedAnonymizedDataCollectionEnabled(mProfile);
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
        return state == InternalState.IDLE
                && mSearchPanel != null
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
     * @param profile The {@link Profile} associated with this Contextual Search session.
     * @return Whether the Contextual Search feature was disabled by the user explicitly.
     */
    static boolean isContextualSearchDisabled(Profile profile) {
        return UserPrefs.get(profile)
                .getString(Pref.CONTEXTUAL_SEARCH_ENABLED)
                .equals(CONTEXTUAL_SEARCH_DISABLED);
    }

    /**
     * @param profile The {@link Profile} associated with this Contextual Search session.
     * @return Whether the Contextual Search feature was enabled by the user explicitly.
     */
    static boolean isContextualSearchEnabled(Profile profile) {
        return UserPrefs.get(profile)
                .getString(Pref.CONTEXTUAL_SEARCH_ENABLED)
                .equals(CONTEXTUAL_SEARCH_ENABLED);
    }

    /**
     * @param profile The {@link Profile} associated with this Contextual Search session.
     * @return Whether the Contextual Search feature is uninitialized (preference unset by the
     *     user).
     */
    static boolean isContextualSearchUninitialized(Profile profile) {
        return UserPrefs.get(profile).getString(Pref.CONTEXTUAL_SEARCH_ENABLED).isEmpty();
    }

    /**
     * @param profile The {@link Profile} associated with this Contextual Search session.
     * @return Whether the Contextual Search fully privacy opt-in was disabled by the user
     *     explicitly.
     */
    static boolean isContextualSearchOptInDisabled(Profile profile) {
        return !UserPrefs.get(profile).getBoolean(Pref.CONTEXTUAL_SEARCH_WAS_FULLY_PRIVACY_ENABLED);
    }

    /**
     * @param profile The {@link Profile} associated with this Contextual Search session.
     * @return Whether the Contextual Search fully privacy opt-in was enabled by the user
     *     explicitly.
     */
    static boolean isContextualSearchOptInEnabled(Profile profile) {
        return UserPrefs.get(profile).getBoolean(Pref.CONTEXTUAL_SEARCH_WAS_FULLY_PRIVACY_ENABLED);
    }

    /**
     * @param profile The {@link Profile} associated with this Contextual Search session.
     * @return Whether the Contextual Search fully privacy opt-in is uninitialized (preference unset
     *     by the user).
     */
    static boolean isContextualSearchOptInUninitialized(Profile profile) {
        return !UserPrefs.get(profile)
                .hasPrefPath(Pref.CONTEXTUAL_SEARCH_WAS_FULLY_PRIVACY_ENABLED);
    }

    /**
     * @param profile The {@link Profile} associated with this Contextual Search session.
     * @return Count of times the promo card has been shown.
     */
    static int getContextualSearchPromoCardShownCount(Profile profile) {
        return UserPrefs.get(profile).getInteger(Pref.CONTEXTUAL_SEARCH_PROMO_CARD_SHOWN_COUNT);
    }

    /**
     * Sets Count of times the promo card has been shown.
     *
     * @param profile The {@link Profile} associated with this Contextual Search session.
     */
    private static void setContextualSearchPromoCardShownCount(Profile profile, int count) {
        UserPrefs.get(profile).setInteger(Pref.CONTEXTUAL_SEARCH_PROMO_CARD_SHOWN_COUNT, count);
    }

    /**
     * @param profile The {@link Profile} associated with this Contextual Search session.
     * @return Whether the Contextual Search feature is disabled when the prefs service considers it
     *     managed.
     */
    static boolean isContextualSearchDisabledByPolicy(Profile profile) {
        return UserPrefs.get(profile).isManagedPreference(Pref.CONTEXTUAL_SEARCH_ENABLED)
                && isContextualSearchDisabled(profile);
    }

    /**
     * Explicitly set whether Contextual Search is enabled or not, with the enabled state being
     * either fully or default-enabled based on previous state. 'enabled' is true - fully opt in or
     * default-enabled based on previous state. 'enabled' is false - the feature is disabled.
     *
     * @param profile The {@link Profile} associated with this Contextual Search session.
     * @param enabled Whether Contextual Search should be enabled.
     */
    static void setContextualSearchState(Profile profile, boolean enabled) {
        @ContextualSearchPreference
        int onState =
                isContextualSearchOptInEnabled(profile)
                        ? ContextualSearchPreference.ENABLED
                        : ContextualSearchPreference.UNINITIALIZED;
        setContextualSearchStateInternal(
                profile, enabled ? onState : ContextualSearchPreference.DISABLED);
    }

    /**
     * @param profile The {@link Profile} associated with this Contextual Search session.
     * @return Whether the Contextual Search feature was fully opted in based on the preference
     *     itself.
     */
    static boolean isContextualSearchPrefFullyOptedIn(Profile profile) {
        return isContextualSearchOptInUninitialized(profile)
                ? isContextualSearchEnabled(profile)
                : isContextualSearchOptInEnabled(profile);
    }

    /**
     * Sets whether the user is fully opted in for Contextual Search Privacy. 'enabled' is true -
     * fully opt in. 'enabled' is false - remain undecided.
     *
     * @param profile The {@link Profile} associated with this Contextual Search session.
     * @param enabled Whether Contextual Search privacy is opted in.
     */
    static void setContextualSearchFullyOptedIn(Profile profile, boolean enabled) {
        UserPrefs.get(profile)
                .setBoolean(Pref.CONTEXTUAL_SEARCH_WAS_FULLY_PRIVACY_ENABLED, enabled);
        setContextualSearchStateInternal(
                profile,
                enabled
                        ? ContextualSearchPreference.ENABLED
                        : ContextualSearchPreference.UNINITIALIZED);
    }

    /** Notifies that a promo card has been shown. */
    static void onPromoShown(Profile profile) {
        int count = getContextualSearchPromoCardShownCount(profile);
        count++;
        setContextualSearchPromoCardShownCount(profile, count);
        ContextualSearchUma.logRevisedPromoOpenCount(count);
    }

    /**
     * @param profile The {@link Profile} associated with this Contextual Search session.
     * @param state The state for the Contextual Search.
     */
    private static void setContextualSearchStateInternal(
            Profile profile, @ContextualSearchPreference int state) {
        PrefService prefs = UserPrefs.get(profile);
        switch (state) {
            case ContextualSearchPreference.UNINITIALIZED:
                prefs.clearPref(Pref.CONTEXTUAL_SEARCH_ENABLED);
                break;
            case ContextualSearchPreference.ENABLED:
                prefs.setString(Pref.CONTEXTUAL_SEARCH_ENABLED, CONTEXTUAL_SEARCH_ENABLED);
                break;
            case ContextualSearchPreference.DISABLED:
                prefs.setString(Pref.CONTEXTUAL_SEARCH_ENABLED, CONTEXTUAL_SEARCH_DISABLED);
                break;
            default:
                Log.e(TAG, "Unexpected state for ContextualSearchPreference state=" + state);
                break;
        }
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

        return isContextualSearchUninitialized(mProfile)
                && isContextualSearchOptInUninitialized(mProfile);
    }

    /**
     * @return Whether a user explicitly enabled the Contextual Search feature.
     */
    boolean isContextualSearchFullyEnabled() {
        if (mDidOverrideFullyEnabledForTesting) return mFullyEnabledForTesting;

        return isContextualSearchResolutionUrlValid() && isContextualSearchEnabled(mProfile);
    }

    /**
     * @return Whether the contextual search resolution URL is valid and can be used to resolve
     *     highlight.
     */
    boolean isContextualSearchResolutionUrlValid() {
        // This function is needed because certain DMA implementations supply a persistent set of
        // Template URL overrides. These overrides are in effect until the user performs a factory
        // data reset of their device, and occasionally miss relevant information, such as - in this
        // particular case - "contextual_search_url" value.
        if (mContextualSearchResolutionUrlValid == null) {
            if (ContextualSearchPolicyJni.get() == null) {
                // JNI is not initialized.
                return false;
            }
            mContextualSearchResolutionUrlValid =
                    ContextualSearchPolicyJni.get().isContextualSearchResolutionUrlValid(mProfile);
        }
        return mContextualSearchResolutionUrlValid;
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

    @NativeMethods
    interface Natives {
        boolean isContextualSearchResolutionUrlValid(@JniType("Profile*") Profile profile);
    }
}
