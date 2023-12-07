// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.SearchEngineType;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.signin.services.UnifiedConsentServiceBridge;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.user_prefs.UserPrefs;

/** Functions for getting the values of ReadAloud feature params. */
public final class ReadAloudFeatures {
    private static final String API_KEY_OVERRIDE_PARAM_NAME = "api_key_override";
    private static final String VOICES_OVERRIDE_PARAM_NAME = "voices_override";

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
        if (profile == null || profile.isOffTheRecord()) {
            return false;
        }
        TemplateUrlService templateUrlService = TemplateUrlServiceFactory.getForProfile(profile);
        TemplateUrl currentSearchEngine = templateUrlService.getDefaultSearchEngineTemplateUrl();
        if (currentSearchEngine == null) {
            return false;
        }
        int searchEngineType =
                templateUrlService.getSearchEngineTypeFromTemplateUrl(
                        currentSearchEngine.getKeyword());

        return searchEngineType == SearchEngineType.SEARCH_ENGINE_GOOGLE
                && ChromeFeatureList.isEnabled(ChromeFeatureList.READALOUD) //
                && UserPrefs.get(profile).getBoolean(Pref.LISTEN_TO_THIS_PAGE_ENABLED)
                // Check whether the user has enabled anonymous URL-keyed data collection.
                // This is surfaced on the relatively new "Make searches and browsing
                // better" user setting.
                && UnifiedConsentServiceBridge.isUrlKeyedAnonymizedDataCollectionEnabled(profile);
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
}
