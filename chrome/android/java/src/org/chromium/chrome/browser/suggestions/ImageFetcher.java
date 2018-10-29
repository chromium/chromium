// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions;

import android.annotation.SuppressLint;
import android.graphics.Bitmap;
import android.os.SystemClock;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.DiscardableReferencePool;
import org.chromium.base.Promise;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.browser.favicon.FaviconHelper;
import org.chromium.chrome.browser.favicon.LargeIconBridge;
import org.chromium.chrome.browser.native_page.NativePageHost;
import org.chromium.chrome.browser.ntp.cards.CardsVariationParameters;
import org.chromium.chrome.browser.ntp.snippets.FaviconFetchResult;
import org.chromium.chrome.browser.ntp.snippets.SnippetArticle;
import org.chromium.chrome.browser.ntp.snippets.SnippetsConfig;
import org.chromium.chrome.browser.ntp.snippets.SuggestionsSource;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.widget.ThumbnailProvider;

import java.net.URI;
import java.net.URISyntaxException;

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
 *   - for article downloads: {@link #makeDownloadThumbnailRequest(SnippetArticle, int)}
 *   - for large icons: {@link #makeLargeIconRequest(String, int,
 * LargeIconBridge.LargeIconCallback)}
 *
 * If there are no errors is the image fetching process, the corresponding bitmap will be returned
 * in the callback. Otherwise, the callback will not be called.
 */
public class ImageFetcher {
    /** Supported favicon sizes in pixels. */
    private static final int[] FAVICON_SERVICE_SUPPORTED_SIZES = {16, 24, 32, 48, 64};
    private static final String FAVICON_SERVICE_FORMAT =
            "https://s2.googleusercontent.com/s2/favicons?domain=%s&src=chrome_newtab_mobile&sz=%d&alt=404";
    /** Min size for site attribution: only 16px as many sites do not have any other small icon. */
    private static final int PUBLISHER_FAVICON_MINIMUM_SIZE_PX = 16;

    /** Desired size for site attribution: only 32px as larger icons are often too complex */
    private static final int PUBLISHER_FAVICON_DESIRED_SIZE_PX = 32;

    private final boolean mUseFaviconService;
    private final NativePageHost mHost;

    private boolean mIsDestroyed;

    private final SuggestionsSource mSuggestionsSource;
    private final Profile mProfile;
    private final DiscardableReferencePool mReferencePool;
    private ThumbnailProvider mThumbnailProvider;
    private FaviconHelper mFaviconHelper;
    private LargeIconBridge mLargeIconBridge;

    public ImageFetcher(SuggestionsSource suggestionsSource, Profile profile,
            DiscardableReferencePool referencePool, NativePageHost host) {
        mSuggestionsSource = suggestionsSource;
        mProfile = profile;
        mReferencePool = referencePool;
        mUseFaviconService = CardsVariationParameters.isFaviconServiceEnabled();
        mHost = host;
    }

    /**
     * Creates a request for a thumbnail from a downloaded image.
     *
     * If there is an error while fetching the thumbnail, the callback will not be called.
     *
     * @param suggestion The suggestion for which we need a thumbnail.
     * @param thumbnailSizePx The required size for the thumbnail.
     * @return The request which will be used to fetch the thumbnail.
     */
    public DownloadThumbnailRequest makeDownloadThumbnailRequest(
            SnippetArticle suggestion, int thumbnailSizePx) {
        assert !mIsDestroyed;

        return new DownloadThumbnailRequest(suggestion, thumbnailSizePx);
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

        if (suggestion.isContextual()) {
            mSuggestionsSource.fetchContextualSuggestionImage(suggestion, callback);
        } else {
            mSuggestionsSource.fetchSuggestionImage(suggestion, callback);
        }
    }

    /**
     * Creates a request for favicon for the URL of a suggestion.
     *
     * If there is an error while fetching the favicon, the callback will not be called.
     *
     * @param suggestion The suggestion whose URL needs a favicon.
     * @param faviconSizePx The required size for the favicon.
     * @param faviconCallback The callback where the bitmap will be returned when fetched.
     */
    public void makeFaviconRequest(SnippetArticle suggestion, final int faviconSizePx,
            final Callback<Bitmap> faviconCallback) {
        assert !mIsDestroyed;

        long faviconFetchStartTimeMs = SystemClock.elapsedRealtime();
        URI pageUrl;

        try {
            pageUrl = new URI(suggestion.getUrl());
        } catch (URISyntaxException e) {
            assert false;
            return;
        }

        if (suggestion.isContextual()
                || (suggestion.isArticle() && SnippetsConfig.isFaviconsFromNewServerEnabled())) {
            // The new code path.
            fetchFaviconFromLocalCacheOrGoogleServer(
                    suggestion, faviconFetchStartTimeMs, faviconCallback);
        } else {
            // The old code path. Currently, we have to use this for non-articles, due to privacy.
            // TODO(jkrcal): Remove this path when we completely switch to the new code on Stable.
            // https://crbug.com/751628
            fetchFaviconFromLocalCache(pageUrl, true, faviconFetchStartTimeMs, faviconSizePx,
                    suggestion, faviconCallback);
        }
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
        if (mFaviconHelper != null) {
            mFaviconHelper.destroy();
            mFaviconHelper = null;
        }

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

    public static String getSnippetDomain(URI snippetUri) {
        return String.format("%s://%s", snippetUri.getScheme(), snippetUri.getHost());
    }

    private void fetchFaviconFromLocalCache(final URI snippetUri, final boolean fallbackToService,
            final long faviconFetchStartTimeMs, final int faviconSizePx,
            final SnippetArticle suggestion, final Callback<Bitmap> faviconCallback) {
        getFaviconHelper().getLocalFaviconImageForURL(mProfile, getSnippetDomain(snippetUri),
                faviconSizePx, new FaviconHelper.FaviconImageCallback() {
                    @Override
                    public void onFaviconAvailable(Bitmap image, String iconUrl) {
                        if (image != null) {
                            assert faviconCallback != null;

                            faviconCallback.onResult(image);
                            recordFaviconFetchHistograms(suggestion,
                                    fallbackToService ? FaviconFetchResult.SUCCESS_CACHED
                                                      : FaviconFetchResult.SUCCESS_FETCHED,
                                    SystemClock.elapsedRealtime() - faviconFetchStartTimeMs);

                            // Update the time when the icon was last requested therefore postponing
                            // the automatic eviction of the favicon from the favicon database.
                            getFaviconHelper().touchOnDemandFavicon(mProfile, iconUrl);
                        } else if (fallbackToService) {
                            if (!fetchFaviconFromService(suggestion, snippetUri,
                                        faviconFetchStartTimeMs, faviconSizePx, faviconCallback)) {
                                recordFaviconFetchHistograms(suggestion, FaviconFetchResult.FAILURE,
                                        SystemClock.elapsedRealtime() - faviconFetchStartTimeMs);
                            }
                        }
                    }
                });
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

    // TODO(crbug.com/635567): Fix this properly.
    @SuppressLint("DefaultLocale")
    private boolean fetchFaviconFromService(final SnippetArticle suggestion, final URI snippetUri,
            final long faviconFetchStartTimeMs, final int faviconSizePx,
            final Callback<Bitmap> faviconCallback) {
        if (!mUseFaviconService) return false;
        int sizePx = getFaviconServiceSupportedSize(faviconSizePx);
        if (sizePx == 0) return false;

        // Replace the default icon by another one from the service when it is fetched.
        ensureIconIsAvailable(
                getSnippetDomain(snippetUri), // Store to the cache for the whole domain.
                String.format(FAVICON_SERVICE_FORMAT, snippetUri.getHost(), sizePx),
                /* isLargeIcon = */ false, new FaviconHelper.IconAvailabilityCallback() {
                    @Override
                    public void onIconAvailabilityChecked(boolean newlyAvailable) {
                        if (!newlyAvailable) {
                            recordFaviconFetchHistograms(suggestion, FaviconFetchResult.FAILURE,
                                    SystemClock.elapsedRealtime() - faviconFetchStartTimeMs);
                            return;
                        }
                        // The download succeeded, the favicon is in the cache; fetch it.
                        fetchFaviconFromLocalCache(snippetUri, /* fallbackToService = */ false,
                                faviconFetchStartTimeMs, faviconSizePx, suggestion,
                                faviconCallback);
                    }
                });
        return true;
    }

    /**
     * Checks if an icon with the given URL is available. If not,
     * downloads it and stores it as a favicon/large icon for the given {@code pageUrl}.
     * @param pageUrl The URL of the site whose icon is being requested.
     * @param iconUrl The URL of the favicon/large icon.
     * @param isLargeIcon Whether the {@code iconUrl} represents a large icon or favicon.
     * @param callback The callback to be notified when the favicon has been checked.
     */
    private void ensureIconIsAvailable(String pageUrl, String iconUrl, boolean isLargeIcon,
            FaviconHelper.IconAvailabilityCallback callback) {
        if (mHost.getActiveTab() != null && mHost.getActiveTab().getWebContents() != null) {
            getFaviconHelper().ensureIconIsAvailable(mProfile,
                    mHost.getActiveTab().getWebContents(), pageUrl, iconUrl, isLargeIcon, callback);
        }
    }

    private int getFaviconServiceSupportedSize(int faviconSizePX) {
        // Take the smallest size larger than mFaviconSizePx.
        for (int size : FAVICON_SERVICE_SUPPORTED_SIZES) {
            if (size > faviconSizePX) return size;
        }
        // Or at least the largest available size (unless too small).
        int largestSize =
                FAVICON_SERVICE_SUPPORTED_SIZES[FAVICON_SERVICE_SUPPORTED_SIZES.length - 1];
        if (faviconSizePX <= largestSize * 1.5) return largestSize;
        return 0;
    }

    private void recordFaviconFetchHistograms(
            SnippetArticle suggestion, int result, long fetchTime) {
        // Record the histogram for articles only to have a fair comparision.
        if (!suggestion.isArticle()) return;
        SuggestionsMetrics.recordArticleFaviconFetchResult(result);
        SuggestionsMetrics.recordArticleFaviconFetchTime(fetchTime);
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
     * Utility method to lazily create the {@link FaviconHelper}, and avoid unnecessary native
     * calls in tests.
     */
    @VisibleForTesting
    protected FaviconHelper getFaviconHelper() {
        if (mFaviconHelper == null) {
            mFaviconHelper = SuggestionsDependencyFactory.getInstance().createFaviconHelper();
        }
        return mFaviconHelper;
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

    /**
     * Request for a download thumbnail.
     *
     * The request uses a {@link Promise<Bitmap>}, which will be fulfilled once the thumbnail is
     * received.
     *
     * Cancellation of the request is available through {@link #cancel(), which will remove the
     * request from the ThumbnailProvider queue}.
     */
    public class DownloadThumbnailRequest implements ThumbnailProvider.ThumbnailRequest {
        private final Promise<Bitmap> mThumbnailReceivedPromise;
        private final SnippetArticle mSuggestion;
        private final int mSize;

        /**
         * @param suggestion The suggestion whose thumbnail will be fetched.
         * @param size The required size for the thumbnail.
         */
        DownloadThumbnailRequest(SnippetArticle suggestion, int size) {
            mThumbnailReceivedPromise = new Promise<>();
            mSuggestion = suggestion;
            mSize = size;

            getThumbnailProvider().getThumbnail(this);
        }

        @Override
        public @Nullable String getFilePath() {
            return mSuggestion.getAssetDownloadFile().getAbsolutePath();
        }

        @Override
        public @Nullable String getContentId() {
            return mSuggestion.getAssetDownloadGuid();
        }

        @Override
        public void onThumbnailRetrieved(@NonNull String contentId, @Nullable Bitmap thumbnail) {
            mThumbnailReceivedPromise.fulfill(thumbnail);
        }

        @Override
        public int getIconSize() {
            return mSize;
        }

        public void cancel() {
            if (mIsDestroyed) return;
            getThumbnailProvider().cancelRetrieval(this);
        }

        public Promise<Bitmap> getPromise() {
            return mThumbnailReceivedPromise;
        }

        @Override
        public String getMimeType() {
            return null;
        }
    }
}
