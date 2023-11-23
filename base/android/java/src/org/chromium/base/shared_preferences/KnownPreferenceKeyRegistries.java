// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.shared_preferences;

import com.google.common.collect.Sets;

import org.chromium.base.ResettersForTesting;
import org.chromium.build.BuildConfig;
import org.chromium.build.annotations.CheckDiscard;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * Ensures that all {@link PreferenceKeyRegistry}s used are known.
 *
 * A complement to ChromePreferenceKeysTest, which ensures that preference keys across all known
 * registries are unique.
 *
 * This checking is done in tests in which |initializeKnownRegistries()| is called, which happens
 * during browser process initialization.
 */
@CheckDiscard("Preference key checking should only happen on build with asserts")
public class KnownPreferenceKeyRegistries {
    private static Set<PreferenceKeyRegistry> sKnownRegistries;
    private static Set<PreferenceKeyRegistry> sRegistriesUsedBeforeInitialization = new HashSet<>();

    public static void onRegistryUsed(PreferenceKeyRegistry registry) {
        if (!BuildConfig.ENABLE_ASSERTS) {
            return;
        }

        if (sKnownRegistries == null) {
            // Before initialization, keep track of registries used.
            sRegistriesUsedBeforeInitialization.add(registry);
        } else {
            // After initialization, check if registry is known.
            if (!sKnownRegistries.contains(registry)) {
                String message =
                        "An unknown registry was used, PreferenceKeyRegistries must be declared as "
                                + "known in AllPreferenceKeyRegistries: "
                                + String.join(",", registry.toDebugString());
                assert false : message;
            }
        }
    }

    public static void initializeKnownRegistries(Set<PreferenceKeyRegistry> knownRegistries) {
        if (!BuildConfig.ENABLE_ASSERTS) {
            return;
        }

        if (sKnownRegistries != null) {
            // Double initialization; make sure new known registries are the same.
            assert sKnownRegistries.equals(knownRegistries);
            return;
        }

        // Check that each registry already used is known; assert otherwise.
        Set<PreferenceKeyRegistry> unknownRegistries =
                Sets.difference(sRegistriesUsedBeforeInitialization, knownRegistries);
        if (!unknownRegistries.isEmpty()) {
            List<String> unknownRegistryNames = new ArrayList<>();
            for (PreferenceKeyRegistry unknownRegistry : unknownRegistries) {
                unknownRegistryNames.add(unknownRegistry.toDebugString());
            }
            String message =
                    "Unknown registries were used, PreferenceKeyRegistries must be declared as "
                            + "known in AllPreferenceKeyRegistries: "
                            + String.join(",", unknownRegistryNames);
            assert false : message;
        }

        sKnownRegistries = knownRegistries;
        sRegistriesUsedBeforeInitialization = null;
    }

    static void clearForTesting() {
        Set<PreferenceKeyRegistry> previousKnownRegistries = sKnownRegistries;
        Set<PreferenceKeyRegistry> registriesUsedBeforeInitialization =
                sRegistriesUsedBeforeInitialization;

        ResettersForTesting.register(
                () -> {
                    sKnownRegistries = previousKnownRegistries;
                    sRegistriesUsedBeforeInitialization = registriesUsedBeforeInitialization;
                });
        sKnownRegistries = null;
        sRegistriesUsedBeforeInitialization = new HashSet<>();
    }
}
