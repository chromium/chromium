// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_group_suggestion;

import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileKeyedMap;

/** This factory creates and keeps a single SuggestionMetricsService per profile. */
@NullMarked
public class SuggestionMetricsServiceFactory {
    private static final ProfileKeyedMap<SuggestionMetricsService> sProfileMap =
            new ProfileKeyedMap<>(ProfileKeyedMap.NO_REQUIRED_CLEANUP_ACTION);
    private static @Nullable SuggestionMetricsService sServiceForTesting;

    private SuggestionMetricsServiceFactory() {}

    @Nullable
    public static SuggestionMetricsService getForProfile(Profile profile) {
        if (sServiceForTesting != null) {
            return sServiceForTesting;
        }

        if (profile.isOffTheRecord()) {
            return null;
        }

        return sProfileMap.getForProfile(profile, SuggestionMetricsServiceFactory::buildForProfile);
    }

    public static void setForTesting(SuggestionMetricsService service) {
        sServiceForTesting = service;
        ResettersForTesting.register(() -> sServiceForTesting = null);
    }

    private static SuggestionMetricsService buildForProfile(Profile profile) {
        return new SuggestionMetricsService();
    }
}
