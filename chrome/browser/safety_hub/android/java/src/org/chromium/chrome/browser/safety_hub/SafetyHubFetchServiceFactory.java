// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import org.chromium.base.ResettersForTesting;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileKeyedMap;

/** Factory for creating {@link SafetyHubFetchService}. */
public class SafetyHubFetchServiceFactory {
    private static ProfileKeyedMap<SafetyHubFetchService> sProfileMap;
    private static SafetyHubFetchService sSafetyHubFetchServiceForTesting;

    /** Return the {@link SafetyHubFetchService} associated with the passed in {@link Profile}. */
    public static SafetyHubFetchService getForProfile(Profile profile) {
        if (sSafetyHubFetchServiceForTesting != null) {
            return sSafetyHubFetchServiceForTesting;
        }

        if (sProfileMap == null) {
            sProfileMap =
                    ProfileKeyedMap.createMapOfDestroyables(
                            ProfileKeyedMap.ProfileSelection.REDIRECTED_TO_ORIGINAL);
        }
        return sProfileMap.getForProfile(profile, SafetyHubFetchService::new);
    }

    public static void setSafetyHubFetchServiceForTesting(
            SafetyHubFetchService safetyHubFetchService) {
        sSafetyHubFetchServiceForTesting = safetyHubFetchService;
        ResettersForTesting.register(() -> sSafetyHubFetchServiceForTesting = null);
    }
}
