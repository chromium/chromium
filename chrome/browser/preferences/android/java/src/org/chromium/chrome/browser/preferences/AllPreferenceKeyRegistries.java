// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.shared_preferences.KnownPreferenceKeyRegistries;
import org.chromium.base.shared_preferences.PreferenceKeyRegistry;
import org.chromium.build.BuildConfig;
import org.chromium.build.annotations.CheckDiscard;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.cached_flags.CachedFlagsSharedPreferences;

import java.util.Set;

@CheckDiscard("Preference key checking should only happen on build with asserts")
@NullMarked
public class AllPreferenceKeyRegistries {
    @VisibleForTesting
    static final @Nullable Set<PreferenceKeyRegistry> KNOWN_REGISTRIES =
            BuildConfig.ENABLE_ASSERTS
                    ? Set.of(
                            ChromeSharedPreferences.REGISTRY,
                            CachedFlagsSharedPreferences.REGISTRY,
                            MultiInstanceSharedPreferences.REGISTRY)
                    : null;

    public static void initializeKnownRegistries() {
        if (KNOWN_REGISTRIES != null) {
            KnownPreferenceKeyRegistries.initializeKnownRegistries(KNOWN_REGISTRIES);
        }
    }
}
