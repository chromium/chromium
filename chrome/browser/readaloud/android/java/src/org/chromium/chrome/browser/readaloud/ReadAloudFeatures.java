// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud;

import android.app.Activity;

import androidx.annotation.Nullable;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.readaloud.ReadAloudMetrics.IneligibilityReason;
import org.chromium.chrome.browser.search_engines.SearchEngineType;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.signin.services.UnifiedConsentServiceBridge;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.user_prefs.UserPrefs;

/** Functions for reading feature flags and params and checking eligibility. */
@JNINamespace("readaloud")
public final class ReadAloudFeatures {
    private static final String API_KEY_OVERRIDE_PARAM_NAME = "api_key_override";
    private static final String VOICES_OVERRIDE_PARAM_NAME = "voices_override";
    private static long sKnownReadableTrialPtr;

    private static @IneligibilityReason int sIneligibilityReason = IneligibilityReason.UNKNOWN;

    /** Perform startup tasks. */
    public static void init() {
        ReadAloudFeaturesJni.get().clearStaleSyntheticTrialPrefs();

        // Prepare the "known readable" synthetic trial. If it was active in a previous session and
        // the trial associated with the "ReadAloud" flag hasn't changed since then, reactivate it
        // now, otherwise it will not be active until activateKnownReadableStudy() is called.
        if (sKnownReadableTrialPtr != 0L) {
            return;
        }
        sKnownReadableTrialPtr =
                ReadAloudFeaturesJni.get()
                        .initSyntheticTrial(ChromeFeatureList.READALOUD, "_KnownReadable");
    }

    /** Destroy native components. */
    public static void shutdown() {
        if (sKnownReadableTrialPtr != 0L) {
            ReadAloudFeaturesJni.get().destroySyntheticTrial(sKnownReadableTrialPtr);
            sKnownReadableTrialPtr = 0L;
        }
    }

    /**
     * Returns true if Read Aloud is allowed. All must be true:
     *
     * <ul>
     *   <li>Feature flag enabled
     *   <li>Not incognito mode
     *   <li>User opted into "Make search and browsing better"
     *   <li>Google is the default search engine
     *   <li>Listen to this page policy is enabled
     * </ul>
     */
    public static boolean isAllowed(Profile profile) {
        sIneligibilityReason = IneligibilityReason.UNKNOWN;
        if (profile == null) {
            return false;
        }

        if (profile.isOffTheRecord()) {
            sIneligibilityReason = IneligibilityReason.INCOGNITO_MODE;
            return false;
        }

        // Check whether the user has enabled anonymous URL-keyed data collection.
        // This is surfaced on the relatively new "Make searches and browsing
        // better" user setting.
        if (!UnifiedConsentServiceBridge.isUrlKeyedAnonymizedDataCollectionEnabled(profile)) {
            sIneligibilityReason = IneligibilityReason.MSBB_DISABLED;
            return false;
        }

        TemplateUrlService templateUrlService = TemplateUrlServiceFactory.getForProfile(profile);
        TemplateUrl currentSearchEngine = templateUrlService.getDefaultSearchEngineTemplateUrl();
        if (currentSearchEngine == null) {
            sIneligibilityReason = IneligibilityReason.DEFAULT_SEARCH_ENGINE_GOOGLE_FALSE;
            return false;
        }
        int searchEngineType =
                templateUrlService.getSearchEngineTypeFromTemplateUrl(
                        currentSearchEngine.getKeyword());
        if (searchEngineType != SearchEngineType.SEARCH_ENGINE_GOOGLE) {
            sIneligibilityReason = IneligibilityReason.DEFAULT_SEARCH_ENGINE_GOOGLE_FALSE;
            return false;
        }

        if (!UserPrefs.get(profile).getBoolean(Pref.LISTEN_TO_THIS_PAGE_ENABLED)) {
            sIneligibilityReason = IneligibilityReason.POLICY_DISABLED;
            return false;
        }

        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.READALOUD)) {
            sIneligibilityReason = IneligibilityReason.FEATURE_FLAG_DISABLED;
            return false;
        }

        return true;
    }

    public static @IneligibilityReason int getIneligibilityReason() {
        return sIneligibilityReason;
    }

    /** Returns true if playback is enabled. */
    public static boolean isPlaybackEnabled() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.READALOUD_PLAYBACK);
    }

    /** Returns true if Read Aloud is allowed to play in the background. */
    public static boolean isBackgroundPlaybackEnabled() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.READALOUD_BACKGROUND_PLAYBACK);
    }

    /** Returns true if Read Aloud entrypoint can be added to overflow menu in CCT. */
    public static boolean isEnabledForOverflowMenuInCCT() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.READALOUD_IN_OVERFLOW_MENU_IN_CCT);
    }

    // TODO: b/323238277 Move this check into isAllowed()
    /** Returns true if in multi-window and ReadAloud is disabled for multi-window. */
    public static boolean isInMultiWindowAndDisabled(Activity activity) {
        return activity.isInMultiWindowMode()
                && !ChromeFeatureList.isEnabled(ChromeFeatureList.READALOUD_IN_MULTI_WINDOW);
    }

    /** Returns true if Read Aloud tap to seek is enabled. */
    public static boolean isTapToSeekEnabled() {
        return ChromeFeatureList.sReadAloudTapToSeek.isEnabled();
    }

    /** Returns true if the ReadAloud CCT IPH should highlight the menu button. */
    public static boolean isIPHMenuButtonHighlightCCTEnabled() {
        return ChromeFeatureList.isEnabled(
                ChromeFeatureList.READALOUD_IPH_MENU_BUTTON_HIGHLIGHT_CCT);
    }

    /** Returns the API key override feature param if present, or null otherwise. */
    @Nullable
    public static String getApiKeyOverride() {
        String apiKeyOverride =
                ChromeFeatureList.getFieldTrialParamByFeature(
                        ChromeFeatureList.READALOUD, API_KEY_OVERRIDE_PARAM_NAME);
        return apiKeyOverride.isEmpty() ? null : apiKeyOverride;
    }

    /**
     * Returns the voice list override param value in serialized form, or empty
     * string if the param is absent. Value is a base64-encoded ListVoicesResponse
     * binarypb.
     */
    public static String getVoicesParam() {
        return ChromeFeatureList.getFieldTrialParamByFeature(
                ChromeFeatureList.READALOUD, VOICES_OVERRIDE_PARAM_NAME);
    }

    /**
     * Activate the "known readable" synthetic trial if it isn't already active. It may be
     * reactivated on the next startup as described above. Only the first call to this method has
     * any effect.
     */
    public static void activateKnownReadableTrial() {
        if (sKnownReadableTrialPtr != 0L) {
            ReadAloudFeaturesJni.get().activateSyntheticTrial(sKnownReadableTrialPtr);
        }
    }

    /** Return the metrics client ID or empty string if it isn't available. */
    public static String getMetricsId() {
        return ReadAloudFeaturesJni.get().getMetricsId();
    }

    /**
     * Returns a string to include with requests to the Read Aloud service to activate experimental
     * features. The string is constructed by combining the trial name and group name of the field
     * trial overriding the "ReadAloudServerExperiments" feature flag (e.g. "Trial_Group"). If no
     * trial overrides the flag, return empty string.
     */
    public static String getServerExperimentFlag() {
        return ReadAloudFeaturesJni.get().getServerExperimentFlag();
    }

    @NativeMethods
    public interface Natives {
        // Create a native readaloud::SyntheticTrial and return its address. It must be
        // cleaned up with destroySyntheticTrial(). May return nullptr if there is no
        // field trial overriding `featureName`.
        long initSyntheticTrial(String featureName, String syntheticTrialNameSuffix);

        // Activate a synthetic trial if not already active. Pointer must not be null.
        void activateSyntheticTrial(long syntheticTrialPtr);

        // Destroy the synthetic trial native object. Pointer must not be null.
        void destroySyntheticTrial(long syntheticTrialPtr);

        // Check stored synthetic trial reactivation prefs and delete those that don't
        // match current field trial state.
        void clearStaleSyntheticTrialPrefs();

        // Get metrics client ID or empty string if it isn't available.
        String getMetricsId();

        // Returns a string to include with requests to the Read Aloud service to activate
        // experimental features.
        String getServerExperimentFlag();
    }
}
