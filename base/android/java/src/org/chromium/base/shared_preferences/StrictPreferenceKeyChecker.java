// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.shared_preferences;

import android.text.TextUtils;

import org.chromium.build.annotations.CheckDiscard;

import java.util.Arrays;
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

    private final PreferenceKeyRegistry mRegistry;

    StrictPreferenceKeyChecker(PreferenceKeyRegistry registry) {
        mRegistry = registry;
    }

    /**
     * Check that the |key| passed is in use.
     * @throws RuntimeException if the key is not in use.
     */
    @Override
    public void checkIsKeyInUse(String key) {
        if (!isKeyInUse(key)) {
            throw new RuntimeException(
                    "SharedPreferences key \""
                            + key
                            + "\" is not registered in PreferenceKeyRegistry.mKeysInUse");
        }
        KnownPreferenceKeyRegistries.onRegistryUsed(mRegistry);
    }

    /**
     * @return Whether |key| is in use.
     */
    private boolean isKeyInUse(String key) {
        // For non-dynamic legacy keys, a simple map check is enough.
        if (mRegistry.mLegacyFormatKeys.contains(key)) {
            return true;
        }

        // For dynamic legacy keys, each legacy prefix has to be checked.
        for (KeyPrefix prefix : mRegistry.mLegacyPrefixes) {
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
            if (!mRegistry.mKeysInUse.contains(prefixFormat)) return false;

            // Check if the dynamic part is correctly formed.
            String dynamicPart = parts[3];
            return DYNAMIC_PART_PATTERN.matcher(dynamicPart).matches();
        } else {
            // Regular key in format "Chrome.[Feature].[Key]" which was not present in |mKeysInUse|.
            // Just check if it is in [keys in use].
            return mRegistry.mKeysInUse.contains(key);
        }
    }

    @Override
    public void checkIsPrefixInUse(KeyPrefix prefix) {
        if (mRegistry.mLegacyPrefixes.contains(prefix)) {
            return;
        }

        if (mRegistry.mKeysInUse.contains(prefix.pattern())) {
            return;
        }

        throw new RuntimeException(
                "SharedPreferences KeyPrefix \""
                        + prefix.pattern()
                        + "\" is not registered in PreferenceKeyRegistry.mKeysInUse()");
    }
}
