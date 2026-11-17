// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences;

import org.chromium.base.shared_preferences.KeyPrefix;
import org.chromium.base.shared_preferences.PreferenceKeyRegistry;
import org.chromium.build.BuildConfig;
import org.chromium.build.annotations.CheckDiscard;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.Collections;
import java.util.List;

/**
 * Contains String and {@link KeyPrefix} constants for per-profile SharedPreferences keys accessed
 * via {@code ProfileSharedPreferences}.
 *
 * <p>New keys must follow the format {@code "Chrome.[Feature].[Key]"} and be added to {@link
 * #getKeysInUse()}. Existing legacy-format keys and prefixes moved from {@link
 * ChromePreferenceKeys#getLegacyKeysInUse()} or {@link
 * ChromePreferenceKeys#getLegacyPrefixesInUse()} belong in {@link #getLegacyKeysInUse()} or {@link
 * #getLegacyPrefixesInUse()}. Every legacy-format entry must also be listed in the OWNERS-gated
 * {@code LegacyChromePreferenceKeys} allowlist, which is enforced by ChromePreferenceKeysTest.
 */
@NullMarked
public final class ProfilePreferenceKeys {
    public static final @Nullable PreferenceKeyRegistry REGISTRY =
            BuildConfig.ENABLE_ASSERTS
                    ? new PreferenceKeyRegistry(
                            "profile_shared_preferences",
                            getKeysInUse(),
                            getLegacyKeysInUse(),
                            getLegacyPrefixesInUse())
                    : null;

    /** Returns the modern {@code "Chrome.[Feature].[Key]"} keys and prefixes in use. */
    @CheckDiscard("Validation is performed in tests and in debug builds.")
    static List<String> getKeysInUse() {
        return Collections.emptyList();
    }

    /** Returns legacy-format per-profile keys migrated from {@link ChromePreferenceKeys}. */
    @CheckDiscard("Validation is performed in tests and in debug builds.")
    static List<String> getLegacyKeysInUse() {
        return Collections.emptyList();
    }

    /**
     * Returns legacy-format per-profile key prefixes migrated from {@link ChromePreferenceKeys}.
     */
    @CheckDiscard("Validation is performed in tests and in debug builds.")
    static List<KeyPrefix> getLegacyPrefixesInUse() {
        return Collections.emptyList();
    }

    private ProfilePreferenceKeys() {}
}
