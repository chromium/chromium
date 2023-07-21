// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.suggestions.mostvisited.MostVisitedSites;
import org.chromium.chrome.browser.suggestions.mostvisited.MostVisitedSitesBridge;
import org.chromium.components.favicon.LargeIconBridge;

/**
 * Provides an injection mechanisms for dependencies of the suggestions package.
 *
 * This class is intended to handle creating the instances of the various classes that interact with
 * native code, so that they can be easily swapped out during tests.
 */
public class SuggestionsDependencyFactory {
    private static SuggestionsDependencyFactory sInstance;

    public static SuggestionsDependencyFactory getInstance() {
        ThreadUtils.assertOnUiThread();
        if (sInstance == null) sInstance = new SuggestionsDependencyFactory();
        return sInstance;
    }

    public static void setInstanceForTesting(SuggestionsDependencyFactory testInstance) {
        if (sInstance != null && testInstance != null) {
            throw new IllegalStateException("A real instance already exists.");
        }
        sInstance = testInstance;
    }

    public MostVisitedSites createMostVisitedSites(Profile profile) {
        return new MostVisitedSitesBridge(profile);
    }

    public LargeIconBridge createLargeIconBridge(Profile profile) {
        return new LargeIconBridge(profile);
    }

    public OfflinePageBridge getOfflinePageBridge(Profile profile) {
        return OfflinePageBridge.getForProfile(profile);
    }
}
