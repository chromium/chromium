// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.Drawable;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.chrome.browser.ui.favicon.FaviconUtils;
import org.chromium.components.browser_ui.widget.RoundedIconGenerator;
import org.chromium.ui.util.ColorUtils;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/** Utility class for fetching favicons in bulk. */
@NullMarked
class BulkFaviconUtil {

    private static final class FaviconFetchRequest {
        private final int mExpectedIcons;
        private final Callback<Map<Integer, Bitmap>> mFaviconCallback;
        private final Map<Integer, Bitmap> mFavicons = new HashMap<>();

        /**
         * @param callback The callback to be called when all favicons are fetched.
         * @param expectedIcons The number of favicons expected to be fetched.
         */
        FaviconFetchRequest(Callback<Map<Integer, Bitmap>> callback, int expectedIcons) {
            mFaviconCallback = callback;
            mExpectedIcons = expectedIcons;
            if (mExpectedIcons == 0) {
                mFaviconCallback.onResult(mFavicons);
            }
        }

        void onFaviconFetched(Integer index, Bitmap icon) {
            mFavicons.put(index, icon);
            if (mFavicons.size() == mExpectedIcons) {
                mFaviconCallback.onResult(mFavicons);
            }
        }
    }

    private @Nullable FaviconHelper mFaviconHelper;
    private FaviconHelper.@Nullable DefaultFaviconHelper mDefaultFaviconHelper;
    private @Nullable RoundedIconGenerator mRoundedIconGenerator;

    BulkFaviconUtil() {}

    /**
     * Fetches favicons for the given list of URLs.
     *
     * @param context The context for fetching fallback icon resources.
     * @param profile The profile to use for fetching favicons.
     * @param webPageUrlList The list of URLs to fetch favicons for.
     * @param size The display size of the favicons to fetch.
     * @param faviconCallback The callback to be called when all favicons are fetched.
     */
    void fetchAsBitmap(
            Context context,
            Profile profile,
            List<GURL> webPageUrlList,
            int size,
            Callback<List<Bitmap>> faviconCallback) {
        boolean isNightMode = ColorUtils.inNightMode(context);
        Callback<Map<Integer, Bitmap>> addFallback =
                (faviconMap) -> {
                    List<Bitmap> results = new ArrayList<>();
                    for (int i = 0; i < faviconMap.size(); ++i) {
                        Bitmap favicon = faviconMap.get(i);
                        if (favicon == null) {
                            favicon =
                                    getDefaultFaviconHelper()
                                            .getDefaultFaviconBitmap(
                                                    context,
                                                    webPageUrlList.get(i),
                                                    /* useDarkIcon= */ !isNightMode,
                                                    /* useIncognitoNtpIcon= */ false);
                        }
                        results.add(favicon);
                    }
                    faviconCallback.onResult(results);
                };

        fetchRawBitmaps(profile, webPageUrlList, size, addFallback);
    }

    /**
     * Fetches favicons for the given list of URLs as Drawables.
     *
     * @param context The context for fetching fallback icon resources.
     * @param profile The profile to use for fetching favicons.
     * @param webPageUrlList The list of webpage URLs to fetch favicons for.
     * @param size The display size of the favicons to fetch.
     * @param faviconCallback The callback to be called when all favicons are fetched.
     */
    void fetchAsDrawable(
            Context context,
            Profile profile,
            List<GURL> webPageUrlList,
            int size,
            Callback<List<Drawable>> faviconCallback) {
        if (mRoundedIconGenerator == null) {
            mRoundedIconGenerator = FaviconUtils.createCircularIconGenerator(context);
        }
        Callback<Map<Integer, Bitmap>> addFallback =
                (faviconMap) -> {
                    List<Drawable> results = new ArrayList<>();
                    for (int i = 0; i < faviconMap.size(); ++i) {
                        Drawable favicon =
                                FaviconUtils.getIconDrawableWithFilter(
                                        faviconMap.get(i),
                                        webPageUrlList.get(i),
                                        mRoundedIconGenerator,
                                        getDefaultFaviconHelper(),
                                        context,
                                        size);
                        results.add(favicon);
                    }
                    faviconCallback.onResult(results);
                };

        fetchRawBitmaps(profile, webPageUrlList, size, addFallback);
    }

    /** Fetches favicons for the given list of URLs as Bitmaps without fallback. */
    private void fetchRawBitmaps(
            Profile profile,
            List<GURL> webPageUrlList,
            int size,
            Callback<Map<Integer, Bitmap>> faviconCallback) {
        FaviconFetchRequest request =
                new FaviconFetchRequest(faviconCallback, webPageUrlList.size());
        for (int i = 0; i < webPageUrlList.size(); ++i) {
            Integer index = i;
            getFaviconHelper()
                    .getForeignFaviconImageForURL(
                            profile,
                            webPageUrlList.get(i),
                            size,
                            (Bitmap bitmap, GURL url) -> {
                                request.onFaviconFetched(index, bitmap);
                            });
        }
    }

    /** Destroys the favicon helper. */
    void destroy() {
        if (mFaviconHelper != null) {
            mFaviconHelper.destroy();
        }
    }

    void setRoundedIconGeneratorForTesting(RoundedIconGenerator generator) {
        mRoundedIconGenerator = generator;
    }

    void setFaviconHelperForTesting(FaviconHelper helper) {
        mFaviconHelper = helper;
    }

    private FaviconHelper.DefaultFaviconHelper getDefaultFaviconHelper() {
        if (mDefaultFaviconHelper == null) {
            mDefaultFaviconHelper = new FaviconHelper.DefaultFaviconHelper();
        }
        return mDefaultFaviconHelper;
    }

    private FaviconHelper getFaviconHelper() {
        if (mFaviconHelper == null) {
            mFaviconHelper = new FaviconHelper();
        }
        return mFaviconHelper;
    }
}
