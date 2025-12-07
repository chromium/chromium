// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.extensions;

import static org.chromium.chrome.browser.profiles.ProfileKeyedMap.ProfileSelection.REDIRECTED_TO_ORIGINAL;

import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileKeyedMap;

/** This factory creates and keeps a single ExtensionsUrlOverrideRegistryManager per profile. */
@NullMarked
public class ExtensionsUrlOverrideRegistryManagerFactory {
    private static final ProfileKeyedMap<ExtensionsUrlOverrideRegistryManager> sProfileMap =
            ProfileKeyedMap.createMapOfDestroyables(REDIRECTED_TO_ORIGINAL);
    private static @Nullable ExtensionsUrlOverrideRegistryManager sServiceForTesting;

    private ExtensionsUrlOverrideRegistryManagerFactory() {}

    @Nullable
    public static ExtensionsUrlOverrideRegistryManager getForProfile(Profile profile) {
        if (sServiceForTesting != null) {
            return sServiceForTesting;
        }

        return sProfileMap.getForProfile(profile, ExtensionsUrlOverrideRegistryManager::new);
    }

    public static void setForTesting(ExtensionsUrlOverrideRegistryManager service) {
        sServiceForTesting = service;
        ResettersForTesting.register(() -> sServiceForTesting = null);
    }
}
