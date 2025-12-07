// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.mostvisited;

import org.jni_zero.CalledByNative;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.suggestions.SiteSuggestion;
import org.chromium.chrome.browser.suggestions.tile.Tile;
import org.chromium.url.GURL;

import java.util.List;

/** Methods to provide most recent urls, titles and thumbnails. */
@NullMarked
public interface MostVisitedSites extends CustomLinkOperations {
    // LINT.IfChange(INVALID_SUGGESTION_SCORE)
    /** Value to indicate that a site suggestion score is unavailable. */
    double INVALID_SUGGESTION_SCORE = -1.0;

    // LINT.ThenChange(//components/ntp_tiles/most_visited_sites.h)

    /** An interface for handling events in {@link MostVisitedSites}. */
    interface Observer {
        /**
         * This is called when the list of most visited URLs is initially available or updated.
         *
         * @param isUserTriggered Whether the event is triggered by direct user action. This is
         *     useful for deciding whether tile update should be eager or deferred.
         * @param siteSuggestions The list of suggested most visited URLs, with more information.
         */
        void onSiteSuggestionsAvailable(
                boolean isUserTriggered, List<SiteSuggestion> siteSuggestions);

        /**
         * This is called when a previously uncached icon has been fetched. Parameters guaranteed to
         * be non-null.
         *
         * @param siteUrl URL of site with newly-cached icon.
         */
        void onIconMadeAvailable(GURL siteUrl);
    }

    /** An interface to provide {@link MostVisitedSites} with platform-specific home page data. */
    interface HomepageClient {
        /**
         * @return True if homepage tile should be shown.
         */
        @CalledByNative("HomepageClient")
        boolean isHomepageTileEnabled();

        /**
         * @return The raw URL of the currently set home page.
         */
        @CalledByNative("HomepageClient")
        @Nullable String getHomepageUrl();
    }

    /** This instance must not be used after calling destroy(). */
    void destroy();

    /**
     * Sets the recipient for events from {@link MostVisitedSites}. The observer may be notified
     * synchronously or asynchronously.
     *
     * @param observer The observer to be notified.
     * @param numSites The maximum number of sites to return.
     */
    void setObserver(Observer observer, int numSites);

    /** Blocklists a URL from the most visited URLs list. */
    void addBlocklistedUrl(GURL url);

    /** Removes a URL from the most visited URLs blocklist. */
    void removeBlocklistedUrl(GURL url);

    /**
     * Records metrics about an impression of the surface with tiles.
     * @param tilesCount Count of tiles available on the surface at the moment.
     */
    void recordPageImpression(int tilesCount);

    /**
     * Records metrics about an impression of a tile including its source (local, server, ...) and
     * its visual type.
     * @param tile Object holding the details of a tile.
     */
    void recordTileImpression(Tile tile);

    /**
     * Records the opening of a Most Visited Item.
     *
     * @param tile Object holding the details of a tile.
     */
    void recordOpenedMostVisitedItem(Tile tile);

    /**
     * Returns the score of a recent suggestion identified by {@param url}, which is {@link
     * INVALID_SUGGESTION_SCORE} if not found.
     */
    double getSuggestionScore(GURL url);
}
