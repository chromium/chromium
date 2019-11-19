// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.mostvisited;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNIAdditionalImport;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.suggestions.SiteSuggestion;
import org.chromium.chrome.browser.suggestions.tile.Tile;

import java.util.ArrayList;
import java.util.Date;
import java.util.List;

/**
 * Methods to bridge into native history to provide most recent urls, titles and thumbnails.
 */
@JNIAdditionalImport(MostVisitedSites.class) // Needed for the Observer usage in the native calls.
public class MostVisitedSitesBridge implements MostVisitedSites {
    /**
     * Maximum number of tiles that is explicitly supported. UMA relies on this value, so even if
     * the UI supports it, getting more can raise unexpected issues.
     */
    public static final int MAX_TILE_COUNT = 12;

    private long mNativeMostVisitedSitesBridge;

    private MostVisitedSites.Observer mWrappedObserver;

    /**
     * MostVisitedSites constructor requires a valid user profile object.
     *
     * @param profile The profile for which to fetch most visited sites.
     */
    public MostVisitedSitesBridge(Profile profile) {
        mNativeMostVisitedSitesBridge =
                MostVisitedSitesBridgeJni.get().init(MostVisitedSitesBridge.this, profile);
    }

    /**
     * Cleans up the C++ side of this class. This instance must not be used after calling destroy().
     */
    @Override
    public void destroy() {
        // Stop listening even if it was not started in the first place. (Handled without errors.)
        assert mNativeMostVisitedSitesBridge != 0;
        MostVisitedSitesBridgeJni.get().destroy(
                mNativeMostVisitedSitesBridge, MostVisitedSitesBridge.this);
        mNativeMostVisitedSitesBridge = 0;
    }

    @Override
    public void setObserver(Observer observer, int numSites) {
        assert numSites <= MAX_TILE_COUNT;
        mWrappedObserver = observer;

        MostVisitedSitesBridgeJni.get().setObserver(
                mNativeMostVisitedSitesBridge, MostVisitedSitesBridge.this, this, numSites);
    }

    @Override
    public void addBlacklistedUrl(String url) {
        if (mNativeMostVisitedSitesBridge == 0) return;
        MostVisitedSitesBridgeJni.get().addOrRemoveBlacklistedUrl(
                mNativeMostVisitedSitesBridge, MostVisitedSitesBridge.this, url, true);
    }

    @Override
    public void removeBlacklistedUrl(String url) {
        if (mNativeMostVisitedSitesBridge == 0) return;
        MostVisitedSitesBridgeJni.get().addOrRemoveBlacklistedUrl(
                mNativeMostVisitedSitesBridge, MostVisitedSitesBridge.this, url, false);
    }

    @Override
    public void recordPageImpression(int tilesCount) {
        if (mNativeMostVisitedSitesBridge == 0) return;
        MostVisitedSitesBridgeJni.get().recordPageImpression(
                mNativeMostVisitedSitesBridge, MostVisitedSitesBridge.this, tilesCount);
    }

    @Override
    public void recordTileImpression(Tile tile) {
        if (mNativeMostVisitedSitesBridge == 0) return;
        MostVisitedSitesBridgeJni.get().recordTileImpression(mNativeMostVisitedSitesBridge,
                MostVisitedSitesBridge.this, tile.getIndex(), tile.getType(), tile.getIconType(),
                tile.getTitleSource(), tile.getSource(),
                tile.getData().dataGenerationTime.getTime(), tile.getUrl());
    }

    @Override
    public void recordOpenedMostVisitedItem(Tile tile) {
        if (mNativeMostVisitedSitesBridge == 0) return;
        MostVisitedSitesBridgeJni.get().recordOpenedMostVisitedItem(mNativeMostVisitedSitesBridge,
                MostVisitedSitesBridge.this, tile.getIndex(), tile.getType(), tile.getTitleSource(),
                tile.getSource(), tile.getData().dataGenerationTime.getTime());
    }

    /**
     * Utility function to convert JNI friendly site suggestion data to a Java friendly list of
     * {@link SiteSuggestion}s.
     */
    public static List<SiteSuggestion> buildSiteSuggestions(String[] titles, String[] urls,
            int[] sections, String[] whitelistIconPaths, int[] titleSources, int[] sources,
            long[] dataGenerationTimesMs) {
        List<SiteSuggestion> siteSuggestions = new ArrayList<>(titles.length);
        for (int i = 0; i < titles.length; ++i) {
            siteSuggestions.add(new SiteSuggestion(titles[i], urls[i], whitelistIconPaths[i],
                    titleSources[i], sources[i], sections[i], new Date(dataGenerationTimesMs[i])));
        }
        return siteSuggestions;
    }

    /**
     * This is called when the list of most visited URLs is initially available or updated.
     * Parameters guaranteed to be non-null.
     *
     * @param titles Array of most visited url page titles.
     * @param urls Array of most visited URLs, including popular URLs if
     *             available and necessary (i.e. there aren't enough most
     *             visited URLs).
     * @param whitelistIconPaths The paths to the icon image files for whitelisted tiles, empty
     *                           strings otherwise.
     * @param sources For each tile, the {@code TileSource} that generated the tile.
     */
    @CalledByNative
    private void onURLsAvailable(String[] titles, String[] urls, int[] sections,
            String[] whitelistIconPaths, int[] titleSources, int[] sources,
            long[] dataGenerationTimesMs) {
        // Don't notify observer if we've already been destroyed.
        if (mNativeMostVisitedSitesBridge == 0) return;

        List<SiteSuggestion> suggestions = new ArrayList<>();

        suggestions.addAll(buildSiteSuggestions(titles, urls, sections, whitelistIconPaths,
                titleSources, sources, dataGenerationTimesMs));

        mWrappedObserver.onSiteSuggestionsAvailable(suggestions);
    }

    /**
     * This is called when a previously uncached icon has been fetched.
     * Parameters guaranteed to be non-null.
     *
     * @param siteUrl URL of site with newly-cached icon.
     */
    @CalledByNative
    private void onIconMadeAvailable(String siteUrl) {
        // Don't notify observer if we've already been destroyed.
        if (mNativeMostVisitedSitesBridge != 0) {
            mWrappedObserver.onIconMadeAvailable(siteUrl);
        }
    }

    @NativeMethods
    interface Natives {
        long init(MostVisitedSitesBridge caller, Profile profile);
        void destroy(long nativeMostVisitedSitesBridge, MostVisitedSitesBridge caller);
        void onHomepageStateChanged(
                long nativeMostVisitedSitesBridge, MostVisitedSitesBridge caller);
        void setHomepageClient(long nativeMostVisitedSitesBridge, MostVisitedSitesBridge caller,
                MostVisitedSites.HomepageClient homePageClient);
        void setObserver(long nativeMostVisitedSitesBridge, MostVisitedSitesBridge caller,
                MostVisitedSitesBridge observer, int numSites);
        void addOrRemoveBlacklistedUrl(long nativeMostVisitedSitesBridge,
                MostVisitedSitesBridge caller, String url, boolean addUrl);
        void recordPageImpression(
                long nativeMostVisitedSitesBridge, MostVisitedSitesBridge caller, int tilesCount);
        void recordTileImpression(long nativeMostVisitedSitesBridge, MostVisitedSitesBridge caller,
                int index, int type, int iconType, int titleSource, int source,
                long dataGenerationTimeMs, String url);
        void recordOpenedMostVisitedItem(long nativeMostVisitedSitesBridge,
                MostVisitedSitesBridge caller, int index, int tileType, int titleSource, int source,
                long dataGenerationTimeMs);
    }
}
