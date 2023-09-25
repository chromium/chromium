// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences;

import android.text.TextUtils;

import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.CheckDiscard;

import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.regex.Pattern;

/**
 * Class that checks if given Strings are valid SharedPreferences keys to use.
 *
 * Checks that:
 * 1. Keys are registered as "in use".
 * 2. The key format is valid, either:
 *   - "Chrome.[Feature].[Key]"
 *   - "Chrome.[Feature].[KeyPrefix].[Suffix]"
 *   - Legacy key prior to this restriction
 */
@CheckDiscard("Validation is performed in tests and in debug builds.")
class StrictPreferenceKeyChecker implements PreferenceKeyChecker {
    // The dynamic part cannot be empty, but otherwise it is anything that does not contain
    // stars.
    private static final Pattern DYNAMIC_PART_PATTERN = Pattern.compile("[^\\*]+");

    private final Set<String> mKeysInUse;
    private final Set<String> mLegacyFormatKeys;
    private final List<KeyPrefix> mLegacyPrefixes;

    /**
     * Constructor that pulls the lists of keys from {@link ChromePreferenceKeys}.
     */
    StrictPreferenceKeyChecker() {
        this(ChromePreferenceKeys.getKeysInUse(), LegacyChromePreferenceKeys.getKeysInUse(),
                LegacyChromePreferenceKeys.getPrefixesInUse());
    }

    /**
     * Generic constructor, dependencies are passed in.
     */
    @VisibleForTesting
    StrictPreferenceKeyChecker(
            List<String> keysInUse, List<String> legacyKeys, List<KeyPrefix> legacyPrefixes) {
        mKeysInUse = new HashSet<>(keysInUse);
        mLegacyFormatKeys = new HashSet<>(legacyKeys);
        mLegacyPrefixes = legacyPrefixes;
    }

    /**
     * Check that the |key| passed is in use.
     * @throws RuntimeException if the key is not in use.
     */
    @Override
    public void checkIsKeyInUse(String key) {
        if (!isKeyInUse(key)) {
            throw new RuntimeException("SharedPreferences key \"" + key
                    + "\" is not registered in ChromePreferenceKeys.createKeysInUse()");
        }
    }

    /**
     * @return Whether |key| is in use.
     */
    private boolean isKeyInUse(String key) {
        // For non-dynamic legacy keys, a simple map check is enough.
        if (mLegacyFormatKeys.contains(key)) {
            return true;
        }

        // For dynamic legacy keys, each legacy prefix has to be checked.
        for (KeyPrefix prefix : mLegacyPrefixes) {
            if (prefix.hasGenerated(key)) {
                return true;
            }
        }

        // If not a format-legacy key, assume it follows the format and find out if it is
        // a prefixed key.
        String[] parts = key.split("\\.", 4);
        if (parts.length < 3) return false;
        boolean isPrefixed = parts.length >= 4;

        if (isPrefixed) {
            // Key with prefix in format "Chrome.[Feature].[KeyPrefix].[Suffix]".

            // Check if its prefix is registered in |mKeysInUse|.
            String prefixFormat =
                    TextUtils.join(".", Arrays.asList(parts[0], parts[1], parts[2], "*"));
            if (!mKeysInUse.contains(prefixFormat)) return false;

            // Check if the dynamic part is correctly formed.
            String dynamicPart = parts[3];
            return DYNAMIC_PART_PATTERN.matcher(dynamicPart).matches();
        } else {
            // Regular key in format "Chrome.[Feature].[Key]" which was not present in |mKeysInUse|.
            // Just check if it is in [keys in use].
            return mKeysInUse.contains(key);
        }
    }

    @Override
    public void checkIsPrefixInUse(KeyPrefix prefix) {
        if (mLegacyPrefixes.contains(prefix)) {
            return;
        }

        if (mKeysInUse.contains(prefix.pattern())) {
            return;
        }

        throw new RuntimeException("SharedPreferences KeyPrefix \"" + prefix.pattern()
                + "\" is not registered in ChromePreferenceKeys.createKeysInUse()");
    }
}
