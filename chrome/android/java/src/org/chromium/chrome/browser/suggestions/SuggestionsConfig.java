// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions;

import android.text.TextUtils;

import androidx.annotation.IntDef;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.browser_ui.widget.displaystyle.UiConfig;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Provides configuration details for suggestions.
 */
public final class SuggestionsConfig {
    @IntDef({TileStyle.MODERN, TileStyle.MODERN_CONDENSED})
    @Retention(RetentionPolicy.SOURCE)
    public @interface TileStyle {
        int MODERN = 1;
        int MODERN_CONDENSED = 2;
    }

    /**
     * Field trial parameter for referrer URL.
     * It must be kept in sync with //components/ntp_suggestions/features.cc
     */
    private static final String REFERRER_URL_PARAM = "referrer_url";

    /**
     * Default value of referrer URL for content suggestions.
     * It must be kept in sync with //components/ntp_suggestions/features.cc
     */
    private static final String DEFAULT_CONTENT_SUGGESTIONS_REFERRER_URL =
            "https://www.google.com/";

    private SuggestionsConfig() {}

    /**
     * Returns the current tile style, that depends on the enabled features and the screen size.
     */
    @TileStyle
    public static int getTileStyle(UiConfig uiConfig) {
        return uiConfig.getCurrentDisplayStyle().isSmall() ? TileStyle.MODERN_CONDENSED
                                                           : TileStyle.MODERN;
    }

    private static boolean useCondensedTileLayout(boolean isScreenSmall) {
        if (isScreenSmall) return true;

        return false;
    }

    /**
     * @param featureName The feature from {@link ChromeFeatureList}, which provides the referrer
     *                    URL parameter.
     * @return The value of referrer URL to use with content suggestions.
     */
    public static String getReferrerUrl(String featureName) {
        assert ChromeFeatureList.INTEREST_FEED_CONTENT_SUGGESTIONS.equals(featureName)
                || ChromeFeatureList.INTEREST_FEED_V2.equals(featureName);

        return getReferrerUrlParamOrDefault(featureName, DEFAULT_CONTENT_SUGGESTIONS_REFERRER_URL);
    }

    private static String getReferrerUrlParamOrDefault(String featureName, String defaultValue) {
        String referrerParamValue =
                ChromeFeatureList.getFieldTrialParamByFeature(featureName, REFERRER_URL_PARAM);

        if (!TextUtils.isEmpty(referrerParamValue)) {
            return referrerParamValue;
        }

        return defaultValue;
    }
}
