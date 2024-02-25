// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.cached_flags.CachedFlagsSharedPreferences;
import org.chromium.base.shared_preferences.KnownPreferenceKeyRegistries;
import org.chromium.base.shared_preferences.PreferenceKeyRegistry;
import org.chromium.build.annotations.CheckDiscard;

import java.util.Set;

@CheckDiscard("Preference key checking should only happen on build with asserts")
public class AllPreferenceKeyRegistries {
    @VisibleForTesting
    static final Set<PreferenceKeyRegistry> KNOWN_REGISTRIES =
            Set.of(ChromeSharedPreferences.REGISTRY, CachedFlagsSharedPreferences.REGISTRY);

    public static void initializeKnownRegistries() {
        KnownPreferenceKeyRegistries.initializeKnownRegistries(KNOWN_REGISTRIES);
    }
}
