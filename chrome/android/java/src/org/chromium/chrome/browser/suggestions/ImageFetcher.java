// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions;

import android.graphics.Bitmap;
import android.os.SystemClock;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.DiscardableReferencePool;
import org.chromium.chrome.browser.favicon.LargeIconBridge;
import org.chromium.chrome.browser.ntp.snippets.SnippetArticle;
import org.chromium.chrome.browser.ntp.snippets.SuggestionsSource;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.widget.ThumbnailProvider;

/**
 * Class responsible for fetching images for the views in the NewTabPage and Chrome Home.
 * The images fetched by this class include:
 *   - favicons for content suggestions
 *   - thumbnails for content suggestions
 *   - large icons for URLs (used by the tiles in the NTP/Chrome Home)
 *
 * To fetch an image, the caller should create a request which is done in the following way:
 *   - for favicons: {@link #makeFaviconRequest(SnippetArticle, int, Callback)}
 *   - for article thumbnails: {@link #makeArticleThumbnailRequest(SnippetArticle, Callback)}
 *   - for large icons: {@link #makeLargeIconRequest(String, int,
 * LargeIconBridge.LargeIconCallback)}
 *
 * If there are no errors is the image fetching process, the corresponding bitmap will be returned
 * in the callback. Otherwise, the callback will not be called.
 */
public class ImageFetcher {
    /** Min size for site attribution: only 16px as many sites do not have any other small icon. */
    static final int PUBLISHER_FAVICON_MINIMUM_SIZE_PX = 16;

    /** Desired size for site attribution: only 32px as larger icons are often too complex */
    static final int PUBLISHER_FAVICON_DESIRED_SIZE_PX = 32;

    private boolean mIsDestroyed;

    private final SuggestionsSource mSuggestionsSource;
    private final Profile mProfile;
    private final DiscardableReferencePool mReferencePool;
    private ThumbnailProvider mThumbnailProvider;
    private LargeIconBridge mLargeIconBridge;

    public ImageFetcher(SuggestionsSource suggestionsSource, Profile profile,
            DiscardableReferencePool referencePool) {
        mSuggestionsSource = suggestionsSource;
        mProfile = profile;
        mReferencePool = referencePool;
    }

    /**
     * Creates a request for an article thumbnail.
     *
     * If there is an error while fetching the thumbnail, the callback will not be called.
     *
     * @param suggestion The article for which a thumbnail is needed.
     * @param callback The callback where the bitmap will be returned when fetched.
     */
    public void makeArticleThumbnailRequest(SnippetArticle suggestion, Callback<Bitmap> callback) {
        assert !mIsDestroyed;

        mSuggestionsSource.fetchSuggestionImage(suggestion, callback);
    }

    /**
     * Creates a request for favicon for the URL of a suggestion.
     *
     * If there is an error while fetching the favicon, the callback will not be called.
     *
     * @param suggestion The suggestion whose URL needs a favicon.
     * @param faviconCallback The callback where the bitmap will be returned when fetched.
     */
    public void makeFaviconRequest(
            SnippetArticle suggestion, final Callback<Bitmap> faviconCallback) {
        assert !mIsDestroyed;
        if (!suggestion.isArticle()) return;

        fetchFaviconFromLocalCacheOrGoogleServer(
                suggestion, SystemClock.elapsedRealtime(), faviconCallback);
    }

    /**
     * Gets the large icon (e.g. favicon or touch icon) for a given URL.
     *
     * If there is an error while fetching the icon, the callback will not be called.
     *
     * @param url The URL of the site whose icon is being requested.
     * @param size The desired size of the icon in pixels.
     * @param callback The callback to be notified when the icon is available.
     */
    public void makeLargeIconRequest(
            String url, int size, LargeIconBridge.LargeIconCallback callback) {
        assert !mIsDestroyed;

        getLargeIconBridge().getLargeIconForUrl(url, size, callback);
    }

    public void onDestroy() {
        if (mLargeIconBridge != null) {
            mLargeIconBridge.destroy();
            mLargeIconBridge = null;
        }

        if (mThumbnailProvider != null) {
            mThumbnailProvider.destroy();
            mThumbnailProvider = null;
        }

        mIsDestroyed = true;
    }

    private void fetchFaviconFromLocalCacheOrGoogleServer(SnippetArticle suggestion,
            final long faviconFetchStartTimeMs, final Callback<Bitmap> faviconCallback) {
        // The bitmap will not be resized to desired size in c++, this only expresses preference
        // as to what image to be fetched from the server.
        mSuggestionsSource.fetchSuggestionFavicon(suggestion, PUBLISHER_FAVICON_MINIMUM_SIZE_PX,
                PUBLISHER_FAVICON_DESIRED_SIZE_PX, new Callback<Bitmap>() {
                    @Override
                    public void onResult(Bitmap image) {
                        SuggestionsMetrics.recordArticleFaviconFetchTime(
                                SystemClock.elapsedRealtime() - faviconFetchStartTimeMs);
                        if (image == null) return;
                        faviconCallback.onResult(image);
                    }
                });
    }

    /**
     * Utility method to lazily create the {@link ThumbnailProvider}, and avoid unnecessary native
     * calls in tests.
     */
    private ThumbnailProvider getThumbnailProvider() {
        if (mThumbnailProvider == null) {
            mThumbnailProvider = SuggestionsDependencyFactory.getInstance().createThumbnailProvider(
                    mReferencePool);
        }
        return mThumbnailProvider;
    }

    /**
     * Utility method to lazily create the {@link LargeIconBridge}, and avoid unnecessary native
     * calls in tests.
     */
    @VisibleForTesting
    protected LargeIconBridge getLargeIconBridge() {
        if (mLargeIconBridge == null) {
            mLargeIconBridge =
                    SuggestionsDependencyFactory.getInstance().createLargeIconBridge(mProfile);
        }
        return mLargeIconBridge;
    }
}
