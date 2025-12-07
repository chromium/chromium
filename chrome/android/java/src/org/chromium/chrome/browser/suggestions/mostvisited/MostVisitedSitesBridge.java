// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.mostvisited;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.suggestions.SiteSuggestion;
import org.chromium.chrome.browser.suggestions.SuggestionsConfig;
import org.chromium.chrome.browser.suggestions.tile.Tile;
import org.chromium.url.GURL;

import java.util.List;

/** Methods to bridge into native history to provide most recent urls, titles and thumbnails. */
@NullMarked
public class MostVisitedSitesBridge implements MostVisitedSites {

    private long mNativeMostVisitedSitesBridge;

    private MostVisitedSites.@Nullable Observer mWrappedObserver;

    /**
     * MostVisitedSites constructor requires a valid user profile object.
     *
     * @param profile The profile for which to fetch most visited sites.
     */
    public MostVisitedSitesBridge(Profile profile) {
        boolean enable_custom_links = ChromeFeatureList.sMostVisitedTilesCustomization.isEnabled();
        mNativeMostVisitedSitesBridge =
                MostVisitedSitesBridgeJni.get().init(profile, enable_custom_links);
    }

    // CustomLinkOperations -> MostVisitedSites implementation.
    @Override
    public boolean addCustomLink(String name, @Nullable GURL url, @Nullable Integer pos) {
        if (mNativeMostVisitedSitesBridge == 0 || GURL.isEmptyOrInvalid(url)) return false;
        if (pos != null) {
            return MostVisitedSitesBridgeJni.get()
                    .addCustomLinkTo(mNativeMostVisitedSitesBridge, name, url, pos.intValue());
        }

        return MostVisitedSitesBridgeJni.get()
                .addCustomLink(mNativeMostVisitedSitesBridge, name, url);
    }

    @Override
    public boolean assignCustomLink(GURL keyUrl, String name, @Nullable GURL url) {
        if (mNativeMostVisitedSitesBridge == 0 || GURL.isEmptyOrInvalid(url)) return false;
        return MostVisitedSitesBridgeJni.get()
                .assignCustomLink(mNativeMostVisitedSitesBridge, keyUrl, name, url);
    }

    @Override
    public boolean deleteCustomLink(GURL keyUrl) {
        if (mNativeMostVisitedSitesBridge == 0) return false;
        return MostVisitedSitesBridgeJni.get()
                .deleteCustomLink(mNativeMostVisitedSitesBridge, keyUrl);
    }

    @Override
    public boolean hasCustomLink(GURL keyUrl) {
        if (mNativeMostVisitedSitesBridge == 0) return false;
        return MostVisitedSitesBridgeJni.get().hasCustomLink(mNativeMostVisitedSitesBridge, keyUrl);
    }

    @Override
    public boolean reorderCustomLink(GURL keyUrl, int newPos) {
        if (mNativeMostVisitedSitesBridge == 0) return false;
        return MostVisitedSitesBridgeJni.get()
                .reorderCustomLink(mNativeMostVisitedSitesBridge, keyUrl, newPos);
    }

    // MostVisitedSites implementation.
    /**
     * Cleans up the C++ side of this class. This instance must not be used after calling destroy().
     */
    @Override
    public void destroy() {
        // Stop listening even if it was not started in the first place. (Handled without errors.)
        assert mNativeMostVisitedSitesBridge != 0;
        MostVisitedSitesBridgeJni.get().destroy(mNativeMostVisitedSitesBridge);
        mNativeMostVisitedSitesBridge = 0;
        mWrappedObserver = null;
    }

    @Override
    public void setObserver(Observer observer, int numSites) {
        assert numSites <= SuggestionsConfig.MAX_TILE_COUNT;
        mWrappedObserver = observer;

        MostVisitedSitesBridgeJni.get().setObserver(mNativeMostVisitedSitesBridge, this, numSites);
    }

    @Override
    public void addBlocklistedUrl(GURL url) {
        if (mNativeMostVisitedSitesBridge == 0) return;
        MostVisitedSitesBridgeJni.get()
                .addOrRemoveBlockedUrl(mNativeMostVisitedSitesBridge, url, true);
    }

    @Override
    public void removeBlocklistedUrl(GURL url) {
        if (mNativeMostVisitedSitesBridge == 0) return;
        MostVisitedSitesBridgeJni.get()
                .addOrRemoveBlockedUrl(mNativeMostVisitedSitesBridge, url, false);
    }

    @Override
    public void recordPageImpression(int tilesCount) {
        if (mNativeMostVisitedSitesBridge == 0) return;
        MostVisitedSitesBridgeJni.get()
                .recordPageImpression(mNativeMostVisitedSitesBridge, tilesCount);
    }

    @Override
    public void recordTileImpression(Tile tile) {
        if (mNativeMostVisitedSitesBridge == 0) return;
        MostVisitedSitesBridgeJni.get()
                .recordTileImpression(
                        mNativeMostVisitedSitesBridge,
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
                        tile.getIndex(),
                        tile.getType(),
                        tile.getTitleSource(),
                        tile.getSource());
    }

    @Override
    public double getSuggestionScore(GURL url) {
        if (mNativeMostVisitedSitesBridge == 0) return MostVisitedSites.INVALID_SUGGESTION_SCORE;
        return MostVisitedSitesBridgeJni.get()
                .getSuggestionScore(mNativeMostVisitedSitesBridge, url);
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
    private void onURLsAvailable(
            boolean isUserTriggered, @JniType("std::vector") List<SiteSuggestion> suggestions) {
        // Don't notify observer if we've already been destroyed.
        if (mNativeMostVisitedSitesBridge != 0 && mWrappedObserver != null) {
            mWrappedObserver.onSiteSuggestionsAvailable(isUserTriggered, suggestions);
        }
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
        if (mNativeMostVisitedSitesBridge != 0 && mWrappedObserver != null) {
            mWrappedObserver.onIconMadeAvailable(siteUrl);
        }
    }

    @NativeMethods
    interface Natives {
        long init(@JniType("Profile*") Profile profile, boolean enableCustomLinks);

        boolean addCustomLinkTo(
                long nativeMostVisitedSitesBridge,
                @JniType("std::u16string") String name,
                @JniType("GURL") GURL url,
                int pos);

        boolean addCustomLink(
                long nativeMostVisitedSitesBridge,
                @JniType("std::u16string") String name,
                @JniType("GURL") GURL url);

        boolean assignCustomLink(
                long nativeMostVisitedSitesBridge,
                @JniType("GURL") GURL keyUrl,
                @JniType("std::u16string") String name,
                @JniType("GURL") GURL url);

        boolean deleteCustomLink(long nativeMostVisitedSitesBridge, @JniType("GURL") GURL keyUrl);

        boolean hasCustomLink(long nativeMostVisitedSitesBridge, @JniType("GURL") GURL keyUrl);

        boolean reorderCustomLink(
                long nativeMostVisitedSitesBridge, @JniType("GURL") GURL keyUrl, int newPos);

        void destroy(long nativeMostVisitedSitesBridge);

        void onHomepageStateChanged(long nativeMostVisitedSitesBridge);

        void setHomepageClient(
                long nativeMostVisitedSitesBridge, MostVisitedSites.HomepageClient homePageClient);

        void setObserver(
                long nativeMostVisitedSitesBridge, MostVisitedSitesBridge observer, int numSites);

        void addOrRemoveBlockedUrl(long nativeMostVisitedSitesBridge, GURL url, boolean addUrl);

        void recordPageImpression(long nativeMostVisitedSitesBridge, int tilesCount);

        void recordTileImpression(
                long nativeMostVisitedSitesBridge,
                int index,
                int type,
                int iconType,
                int titleSource,
                int source,
                GURL url);

        void recordOpenedMostVisitedItem(
                long nativeMostVisitedSitesBridge,
                int index,
                int tileType,
                int titleSource,
                int source);

        double getSuggestionScore(long nativeMostVisitedSitesBridge, @JniType("GURL") GURL url);
    }
}
