// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud;

import com.google.common.collect.ImmutableList;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
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

import java.util.List;

/** Functions for reading feature flags and params and checking eligibility. */
@JNINamespace("readaloud")
@NullMarked
public final class ReadAloudFeatures {
    private static final String API_KEY_OVERRIDE_PARAM_NAME = "api_key_override";
    private static final String VOICES_OVERRIDE_PARAM_NAME = "voices_override";

    private static @IneligibilityReason int sIneligibilityReason = IneligibilityReason.UNKNOWN;

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

    public static boolean isAudioOverviewsAllowed() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.READALOUD_AUDIO_OVERVIEWS);
    }

    public static boolean isAudioOverviewsFeedbackAllowed() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.READALOUD_AUDIO_OVERVIEWS_FEEDBACK);
    }

    public static int getAudioOverviewsSpeedAdditionPercentage() {
        return ChromeFeatureList.sReadAloudAudioOverviewsSpeedAdditionPercentage.getValue();
    }

    public static boolean shouldConsiderLanguageInOverviewReadability() {
      return ChromeFeatureList.sShouldConsiderLanguageInOverviewReadability.getValue();
    }

    public static int getReadabilityDelayMsAfterPageLoad() {
      return ChromeFeatureList.sReadAloudReadabilityDelayMsAfterPageLoad.getValue();
    }

    public static @IneligibilityReason int getIneligibilityReason() {
        return sIneligibilityReason;
    }

    /** Returns true if playback is enabled. */
    public static boolean isPlaybackEnabled() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.READALOUD_PLAYBACK);
    }

    /** Returns true if the ReadAloud CCT IPH should highlight the menu button. */
    public static boolean isIPHMenuButtonHighlightCctEnabled() {
        return ChromeFeatureList.isEnabled(
                ChromeFeatureList.READALOUD_IPH_MENU_BUTTON_HIGHLIGHT_CCT);
    }

    /** Returns the API key override feature param if present, or null otherwise. */
    public static @Nullable String getApiKeyOverride() {
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

    public static List<String> getSupportedLanguagesForOverview() {
        ImmutableList.Builder<String> result = ImmutableList.builder();
        for (String language :
                ChromeFeatureList.sReadAloudAudioOverviewsSupportedLanguages
                        .getValue()
                        .split(",")) {
            String trimmed = language.trim();
            if (!trimmed.isEmpty()) {
                result.add(trimmed);
            }
      }
      return result.build();
    }

    @NativeMethods
    public interface Natives {
        // Get metrics client ID or empty string if it isn't available.
        String getMetricsId();

        // Returns a string to include with requests to the Read Aloud service to activate
        // experimental features.
        String getServerExperimentFlag();
    }
}
