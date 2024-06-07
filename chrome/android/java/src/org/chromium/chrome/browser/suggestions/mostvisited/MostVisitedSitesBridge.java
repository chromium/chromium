// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.mostvisited;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.suggestions.SiteSuggestion;
import org.chromium.chrome.browser.suggestions.tile.Tile;
import org.chromium.url.GURL;

import java.util.List;

/** Methods to bridge into native history to provide most recent urls, titles and thumbnails. */
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
        MostVisitedSitesBridgeJni.get()
                .destroy(mNativeMostVisitedSitesBridge, MostVisitedSitesBridge.this);
        mNativeMostVisitedSitesBridge = 0;
        mWrappedObserver = null;
    }

    @Override
    public void setObserver(Observer observer, int numSites) {
        assert numSites <= MAX_TILE_COUNT;
        mWrappedObserver = observer;

        MostVisitedSitesBridgeJni.get()
                .setObserver(
                        mNativeMostVisitedSitesBridge, MostVisitedSitesBridge.this, this, numSites);
    }

    @Override
    public void addBlocklistedUrl(GURL url) {
        if (mNativeMostVisitedSitesBridge == 0) return;
        MostVisitedSitesBridgeJni.get()
                .addOrRemoveBlockedUrl(
                        mNativeMostVisitedSitesBridge, MostVisitedSitesBridge.this, url, true);
    }

    @Override
    public void removeBlocklistedUrl(GURL url) {
        if (mNativeMostVisitedSitesBridge == 0) return;
        MostVisitedSitesBridgeJni.get()
                .addOrRemoveBlockedUrl(
                        mNativeMostVisitedSitesBridge, MostVisitedSitesBridge.this, url, false);
    }

    @Override
    public void recordPageImpression(int tilesCount) {
        if (mNativeMostVisitedSitesBridge == 0) return;
        MostVisitedSitesBridgeJni.get()
                .recordPageImpression(
                        mNativeMostVisitedSitesBridge, MostVisitedSitesBridge.this, tilesCount);
    }

    @Override
    public void recordTileImpression(Tile tile) {
        if (mNativeMostVisitedSitesBridge == 0) return;
        MostVisitedSitesBridgeJni.get()
                .recordTileImpression(
                        mNativeMostVisitedSitesBridge,
                        MostVisitedSitesBridge.this,
                        tile.getIndex(),
                        tile.getType(),
                        tile.getIconType(),
                        tile.getTitleSource(),
                        tile.getSource(),
                        tile.getUrl());
    }

    @Override
    public void recordOpenedMostVisitedItem(Tile tile) {
        if (mNativeMostVisitedSitesBridge == 0) return;
        MostVisitedSitesBridgeJni.get()
                .recordOpenedMostVisitedItem(
                        mNativeMostVisitedSitesBridge,
                        MostVisitedSitesBridge.this,
                        tile.getIndex(),
                        tile.getType(),
                        tile.getTitleSource(),
                        tile.getSource());
    }

    @CalledByNative
    private static SiteSuggestion makeSiteSuggestion(
            @JniType("std::u16string") String title,
            @JniType("GURL") GURL url,
            int titleSource,
            int source,
            int section) {
        return new SiteSuggestion(title, url, titleSource, source, section);
    }

    /**
     * This is called when the list of most visited URLs is initially available or updated.
     * Parameters guaranteed to be non-null.
     */
    @CalledByNative
    private void onURLsAvailable(@JniType("std::vector") List<SiteSuggestion> suggestions) {
        // Don't notify observer if we've already been destroyed.
        if (mNativeMostVisitedSitesBridge == 0) return;

        mWrappedObserver.onSiteSuggestionsAvailable(suggestions);
    }

    /**
     * This is called when a previously uncached icon has been fetched. Parameters guaranteed to be
     * non-null.
     *
     * @param siteUrl URL of site with newly-cached icon.
     */
    @CalledByNative
    private void onIconMadeAvailable(@JniType("GURL") GURL siteUrl) {
        // Don't notify observer if we've already been destroyed.
        if (mNativeMostVisitedSitesBridge != 0) {
            mWrappedObserver.onIconMadeAvailable(siteUrl);
        }
    }

    @NativeMethods
    interface Natives {
        long init(MostVisitedSitesBridge caller, @JniType("Profile*") Profile profile);

        void destroy(long nativeMostVisitedSitesBridge, MostVisitedSitesBridge caller);

        void onHomepageStateChanged(
                long nativeMostVisitedSitesBridge, MostVisitedSitesBridge caller);

        void setHomepageClient(
                long nativeMostVisitedSitesBridge,
                MostVisitedSitesBridge caller,
                MostVisitedSites.HomepageClient homePageClient);

        void setObserver(
                long nativeMostVisitedSitesBridge,
                MostVisitedSitesBridge caller,
                MostVisitedSitesBridge observer,
                int numSites);

        void addOrRemoveBlockedUrl(
                long nativeMostVisitedSitesBridge,
                MostVisitedSitesBridge caller,
                GURL url,
                boolean addUrl);

        void recordPageImpression(
                long nativeMostVisitedSitesBridge, MostVisitedSitesBridge caller, int tilesCount);

        void recordTileImpression(
                long nativeMostVisitedSitesBridge,
                MostVisitedSitesBridge caller,
                int index,
                int type,
                int iconType,
                int titleSource,
                int source,
                GURL url);

        void recordOpenedMostVisitedItem(
                long nativeMostVisitedSitesBridge,
                MostVisitedSitesBridge caller,
                int index,
                int tileType,
                int titleSource,
                int source);
    }
}
