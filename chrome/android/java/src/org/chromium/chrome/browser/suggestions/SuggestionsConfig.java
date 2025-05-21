// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions;

import androidx.annotation.IntDef;

import org.chromium.components.browser_ui.widget.displaystyle.UiConfig;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Provides configuration details for suggestions. */
public final class SuggestionsConfig {
    @IntDef({TileStyle.MODERN, TileStyle.MODERN_CONDENSED})
    @Retention(RetentionPolicy.SOURCE)
    public @interface TileStyle {
        int MODERN = 1;
        int MODERN_CONDENSED = 2;
    }

    /**
     * Maximum number of tiles that is explicitly supported. UMA relies on this value, so even if
     * the UI supports it, getting more can raise unexpected issues.
     */
    public static final int MAX_TILE_COUNT = 12;

    /** Maximum number of custom tiles supported. In C++ backend this is `kMaxNumCustomLinks`. */
    public static final int MAX_NUM_CUSTOM_LINKS = 8;

    /** Maximum length of Custom Tiles name. */
    public static final int MAX_CUSTOM_TILES_NAME_LENGTH = 50;

    /** Maximum length of Custom Tiles URL. */
    public static final int MAX_CUSTOM_TILES_URL_LENGTH = 2083;

    /**
     * Default value of referrer URL for content suggestions. It must be kept in sync with
     * //components/feed/feed_feature_list.cc.
     */
    private static final String DEFAULT_CONTENT_SUGGESTIONS_REFERRER_URL =
            "https://www.google.com/";

    private SuggestionsConfig() {}

    /** Returns the current tile style, that depends on the enabled features and the screen size. */
    public static @TileStyle int getTileStyle(UiConfig uiConfig) {
        return uiConfig.getCurrentDisplayStyle().isSmall()
                ? TileStyle.MODERN_CONDENSED
                : TileStyle.MODERN;
    }

    /**
     * @return The value of referrer URL to use with content suggestions.
     */
    public static String getReferrerUrl() {
        return DEFAULT_CONTENT_SUGGESTIONS_REFERRER_URL;
    }
}
