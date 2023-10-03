// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.UnifiedConsentServiceBridge;

/** Functions for getting the values of ReadAloud feature params. */
public final class ReadAloudFeatures {
    private static final String API_KEY_OVERRIDE_PARAM_NAME = "api_key_override";
    private static final String VOICES_OVERRIDE_PARAM_NAME = "voices_override";

    /**
     * Returns true if Read Aloud is allowed. All must be true: - Feature flag enabled - Not
     * incognito mode - User opted into "Make search and browsing better"
     */
    public static boolean isAllowed(Profile profile) {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.READALOUD) //
                && profile != null //
                && !profile.isOffTheRecord() //
                // Check whether the user has enabled anonymous URL-keyed data collection.
                // This is surfaced on the relatively new "Make searches and browsing better"
                // user setting.
                && UnifiedConsentServiceBridge.isUrlKeyedAnonymizedDataCollectionEnabled(profile);
    }

    /** Returns the API key override feature param if present, or null otherwise. */
    @Nullable
    public static String getApiKeyOverride() {
        String apiKeyOverride = ChromeFeatureList.getFieldTrialParamByFeature(
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
