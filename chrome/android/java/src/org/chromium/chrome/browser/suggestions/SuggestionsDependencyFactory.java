// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions;

import static org.chromium.chrome.browser.ChromeFeatureList.INTEREST_FEED_CONTENT_SUGGESTIONS;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.DiscardableReferencePool;
import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.favicon.FaviconHelper;
import org.chromium.chrome.browser.favicon.LargeIconBridge;
import org.chromium.chrome.browser.ntp.snippets.EmptySuggestionsSource;
import org.chromium.chrome.browser.ntp.snippets.SnippetsBridge;
import org.chromium.chrome.browser.ntp.snippets.SuggestionsSource;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.suggestions.mostvisited.MostVisitedSites;
import org.chromium.chrome.browser.suggestions.mostvisited.MostVisitedSitesBridge;
import org.chromium.chrome.browser.widget.ThumbnailProvider;
import org.chromium.chrome.browser.widget.ThumbnailProviderImpl;

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

    @VisibleForTesting
    public static void setInstanceForTesting(SuggestionsDependencyFactory testInstance) {
        if (sInstance != null && testInstance != null) {
            throw new IllegalStateException("A real instance already exists.");
        }
        sInstance = testInstance;
    }

    public SuggestionsSource createSuggestionSource(Profile profile) {
        return ChromeFeatureList.isEnabled(INTEREST_FEED_CONTENT_SUGGESTIONS)
                ? new EmptySuggestionsSource()
                : new SnippetsBridge(profile);
    }

    public SuggestionsEventReporter createEventReporter() {
        return new SuggestionsEventReporterBridge();
    }

    public MostVisitedSites createMostVisitedSites(Profile profile) {
        return new MostVisitedSitesBridge(profile);
    }

    public LargeIconBridge createLargeIconBridge(Profile profile) {
        return new LargeIconBridge(profile);
    }

    public ThumbnailProvider createThumbnailProvider(DiscardableReferencePool referencePool) {
        return new ThumbnailProviderImpl(
                referencePool, ThumbnailProviderImpl.ClientType.NTP_SUGGESTIONS);
    }

    public FaviconHelper createFaviconHelper() {
        return new FaviconHelper();
    }

    public OfflinePageBridge getOfflinePageBridge(Profile profile) {
        return OfflinePageBridge.getForProfile(profile);
    }
}
