// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.language.settings;

import org.chromium.base.BundleUtils;
import org.chromium.base.LocaleUtils;
import org.chromium.chrome.browser.ProductConfig;

import java.util.Arrays;
import java.util.Comparator;

/**
 * Utility to determine if a language code is available as the UI language.
 * Uses the lists from ProductConfig to stay in sync with compiled resources.
 */
public class AvailableUiLanguages {
    private AvailableUiLanguages() {}

    /**
     * Get available language set from {@link ProductConfig}, which is a sorted
     * array of language tags included in this build.
     */
    private static String[] getLanguageList() {
        if (BundleUtils.isBundle()) {
            return ProductConfig.UNCOMPRESSED_LOCALES;
        }
        return ProductConfig.COMPRESSED_LOCALES;
    }

    /**
     * Return true if the language is available as the UI language. This is used to disable
     * the overflow option when selecting a UI language.
     * @param language BCP-47 language tag representing a locale (e.g. "en-US")
     */
    public static boolean isAvailable(String language) {
        return Arrays.binarySearch(getLanguageList(), language, LANGUAGE_COMPARATOR) >= 0;
    }

    /**
     * Comparator that removes any country or script information from either language tag
     * since they are not needed for locale availability checks.
     * Example: "es-MX" and "es-ES" will evaluate as equal.
     */
    private static final Comparator<String> LANGUAGE_COMPARATOR = new Comparator<String>() {
        @Override
        public int compare(String a, String b) {
            String langA = LocaleUtils.toLanguage(a);
            String langB = LocaleUtils.toLanguage(b);
            return langA.compareTo(langB);
        }
    };
}
