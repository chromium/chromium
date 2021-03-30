// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.omnibox.status.StatusProperties.StatusIconResource;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.theme.ThemeUtils;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.components.browser_ui.widget.RoundedIconGenerator;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.content_public.browser.BrowserStartupController;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.HashMap;
import java.util.Map;

/**
 * Collection of shared code for displaying search engine logos.
 */
public class SearchEngineLogoUtils {
    // Note: shortened to account for the 20 character limit.
    private static final String TAG = "SearchLogoUtils";
    private static final String ROUNDED_EDGES_VARIANT = "rounded_edges";
    private static final String LOUPE_EVERYWHERE_VARIANT = "loupe_everywhere";
    private static final String DUMMY_URL_QUERY = "replace_me";

    private static SearchEngineLogoUtils sInstance;
    // Cached values to prevent duplicate work.
    private static Bitmap sCachedComposedBackground;
    private static String sCachedComposedBackgroundLogoUrl;
    private static int sSearchEngineLogoTargetSizePixels;
    private static int sSearchEngineLogoComposedSizePixels;

    /** Get the singleton instance of SearchEngineLogoUtils */
    public static SearchEngineLogoUtils getInstance() {
        ThreadUtils.assertOnUiThread();
        if (sInstance == null) {
            sInstance = new SearchEngineLogoUtils(BrowserStartupController.getInstance());
        }
        return sInstance;
    }

    private final BrowserStartupController mBrowserStartupController;
    // Lazy initialization for native-bound dependencies.
    private FaviconHelper mFaviconHelper;
    private RoundedIconGenerator mRoundedIconGenerator;

    /**
     * AndroidSearchEngineLogoEvents defined in tools/metrics/histograms/enums.xml. These values
     * are persisted to logs. Entries should not be renumbered and numeric values should never be
     * reused.
     */
    @IntDef({Events.FETCH_NON_GOOGLE_LOGO_REQUEST, Events.FETCH_FAILED_NULL_URL,
            Events.FETCH_FAILED_FAVICON_HELPER_ERROR, Events.FETCH_FAILED_RETURNED_BITMAP_NULL,
            Events.FETCH_SUCCESS_CACHE_HIT, Events.FETCH_SUCCESS})
    @Retention(RetentionPolicy.SOURCE)
    public @interface Events {
        int FETCH_NON_GOOGLE_LOGO_REQUEST = 0;
        int FETCH_FAILED_NULL_URL = 1;
        int FETCH_FAILED_FAVICON_HELPER_ERROR = 2;
        int FETCH_FAILED_RETURNED_BITMAP_NULL = 3;
        int FETCH_SUCCESS_CACHE_HIT = 4;
        int FETCH_SUCCESS = 5;

        int MAX = 6;
    }

    @VisibleForTesting
    SearchEngineLogoUtils(BrowserStartupController browserStartupController) {
        mBrowserStartupController = browserStartupController;
    }

    /**
     * Encapsulates complicated boolean check for reuse and readability.
     * @return True if the search engine logo is enabled, regardless of visibility.
     */
    public boolean isSearchEngineLogoEnabled() {
        // LocaleManager#needToCheckForSearchEnginePromo() checks several system features which
        // risk throwing exceptions. See the exception cases below for details.
        try {
            return !LocaleManager.getInstance().needToCheckForSearchEnginePromo()
                    && ChromeFeatureList.isInitialized()
                    && ChromeFeatureList.isEnabled(ChromeFeatureList.OMNIBOX_SEARCH_ENGINE_LOGO);
        } catch (SecurityException e) {
            Log.e(TAG, "Can thrown by failed IPC, see crbug.com/1027709");
            return false;
        } catch (RuntimeException e) {
            Log.e(TAG, "Can be thrown if underlying services are dead, see crbug.com/1121602");
            return false;
        }
    }

    /**
     * Encapsulates complicated boolean check for reuse and readability.
     * @param isOffTheRecord True if the user is currently using an incognito tab.
     * @return True if we should show the search engine logo.
     */
    public boolean shouldShowSearchEngineLogo(boolean isOffTheRecord) {
        return !isOffTheRecord
                && isSearchEngineLogoEnabled()
                // Using the profile now, so we need to pay attention to browser initialization.
                && mBrowserStartupController.isFullBrowserStarted();
    }

    /**
     * Encapsulates complicated boolean check for reuse and readability.
     * @param isOffTheRecord True if the user is currently using an incognito tab.
     * @return True if we should show the rounded search engine logo.
     */
    public boolean shouldShowRoundedSearchEngineLogo(boolean isOffTheRecord) {
        return shouldShowSearchEngineLogo(isOffTheRecord) && ChromeFeatureList.isInitialized()
                && ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                        ChromeFeatureList.OMNIBOX_SEARCH_ENGINE_LOGO, ROUNDED_EDGES_VARIANT,
                        /* default= */ true);
    }

    /** Ignores the incognito state for instances where a caller would otherwise pass "false". */
    boolean isRoundedSearchEngineLogoEnabled() {
        return shouldShowRoundedSearchEngineLogo(false);
    }

    /**
     * Encapsulates complicated boolean check for reuse and readability.
     * @param isOffTheRecord True if the user is currently using an incognito tab.
     * @return True if we should show the search engine logo as a loupe everywhere.
     */
    public boolean shouldShowSearchLoupeEverywhere(boolean isOffTheRecord) {
        return shouldShowSearchEngineLogo(isOffTheRecord)
                && ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                        ChromeFeatureList.OMNIBOX_SEARCH_ENGINE_LOGO, LOUPE_EVERYWHERE_VARIANT,
                        /* default= */ false);
    }

    /** @return Whether the status icon should be hidden when the LocationBar is unfocused. */
    public boolean currentlyOnNTP(LocationBarDataProvider locationBarDataProvider) {
        return locationBarDataProvider != null
                && UrlUtilities.isNTPUrl(locationBarDataProvider.getCurrentUrl());
    }

    /**
     * @param templateUrlService The TemplateUrlService to use to derive the logo url.
     * Returns the search URL of the current DSE or null if one cannot be found.
     */
    @Nullable
    public String getSearchLogoUrl(TemplateUrlService templateUrlService) {
        String logoUrlWithPath = templateUrlService.getUrlForSearchQuery(DUMMY_URL_QUERY);
        if (logoUrlWithPath == null || !UrlUtilities.isHttpOrHttps(logoUrlWithPath)) {
            return logoUrlWithPath;
        }

        return UrlUtilities.stripPath(logoUrlWithPath);
    }

    /**
     * @param resources Android resources object, used to read the dimension.
     * @return The size that the logo favicon should be.
     */
    public int getSearchEngineLogoSizePixels(Resources resources) {
        if (sSearchEngineLogoTargetSizePixels == 0) {
            if (isRoundedSearchEngineLogoEnabled()) {
                sSearchEngineLogoTargetSizePixels = resources.getDimensionPixelSize(
                        R.dimen.omnibox_search_engine_logo_favicon_size);
            } else {
                sSearchEngineLogoTargetSizePixels =
                        getSearchEngineLogoComposedSizePixels(resources);
            }
        }

        return sSearchEngineLogoTargetSizePixels;
    }

    /**
     * @param resources Android resources object, used to read the dimension.
     * @return The total size the logo will be on screen.
     */
    public static int getSearchEngineLogoComposedSizePixels(Resources resources) {
        if (sSearchEngineLogoComposedSizePixels == 0) {
            sSearchEngineLogoComposedSizePixels = resources.getDimensionPixelSize(
                    R.dimen.omnibox_search_engine_logo_composed_size);
        }

        return sSearchEngineLogoComposedSizePixels;
    }

    /**
     * Get the search engine logo favicon. This can return a null bitmap under certain
     * circumstances, such as: no logo url found, network/cache error, etc.
     *
     * @param resources Provides access to Android resources.
     * @param inNightMode Whether the device is currently in night mode, used to tint icons.
     * @param profile The current profile. When null, falls back to locally-provided icons.
     * @param templateUrlService The current templateUrlService. When null, falls back to
     *         locally-provided icons.
     * @param callback How the bitmap will be returned to the caller.
     */
    public void getSearchEngineLogo(@NonNull Resources resources, boolean inNightMode,
            @Nullable Profile profile, @Nullable TemplateUrlService templateUrlService,
            @NonNull Callback<StatusIconResource> callback) {
        // If either of the nullable dependencies are null, fallback to the search loupe. If
        // templateUrlService is available and the default search engine is Google, serve that
        // locally.
        if (profile == null || templateUrlService == null) {
            callback.onResult(getSearchLoupeResource(inNightMode));
            return;
        } else if (templateUrlService.isDefaultSearchEngineGoogle()) {
            callback.onResult(new StatusIconResource(R.drawable.ic_logo_googleg_20dp, 0));
            return;
        }

        // If all of the nullable dependencies are present and the search engine is non-Google,
        // then go to the network to fetch the icon.
        recordEvent(Events.FETCH_NON_GOOGLE_LOGO_REQUEST);
        if (mFaviconHelper == null) mFaviconHelper = new FaviconHelper();

        String logoUrl = getSearchLogoUrl(templateUrlService);
        if (logoUrl == null) {
            callback.onResult(getSearchLoupeResource(inNightMode));
            recordEvent(Events.FETCH_FAILED_NULL_URL);
            return;
        }

        // Return a cached copy if it's available.
        if (sCachedComposedBackground != null && sCachedComposedBackgroundLogoUrl.equals(logoUrl)) {
            callback.onResult(new StatusIconResource(logoUrl, sCachedComposedBackground, 0));
            recordEvent(Events.FETCH_SUCCESS_CACHE_HIT);
            return;
        }

        final int logoSizePixels = getSearchEngineLogoSizePixels(resources);
        boolean willCallbackBeCalled = mFaviconHelper.getLocalFaviconImageForURL(
                profile, logoUrl, logoSizePixels, (image, iconUrl) -> {
                    if (image == null) {
                        callback.onResult(getSearchLoupeResource(inNightMode));
                        recordEvent(Events.FETCH_FAILED_RETURNED_BITMAP_NULL);
                        return;
                    }

                    processReturnedLogo(logoUrl, image, resources, callback);
                    recordEvent(Events.FETCH_SUCCESS);
                });
        if (!willCallbackBeCalled) {
            callback.onResult(getSearchLoupeResource(inNightMode));
            recordEvent(Events.FETCH_FAILED_FAVICON_HELPER_ERROR);
        }
    }

    @VisibleForTesting
    StatusIconResource getSearchLoupeResource(boolean inNightMode) {
        return new StatusIconResource(R.drawable.ic_search,
                inNightMode ? R.color.default_icon_color_secondary_tint_list
                            : ThemeUtils.getThemedToolbarIconTintRes(/* useLight= */ true));
    }

    /**
     * Process the image returned from a network fetch or cache hit. This method processes the logo
     * to make it eligible for display. The logo is resized to ensure it will fill the required
     * size. This is done because the icon returned from native could be a different size. If the
     * rounded edges variant is active, then a smaller icon is downloaded and drawn on top of a
     * circle background. This looks better and also has more predictable behavior than rounding the
     * edges of the full size icon. The circle background is a solid color made up of the result
     * from a call to getMostCommonEdgeColor(...).
     * @param logoUrl The url for the given logo.
     * @param image The logo to process.
     * @param resources Android resources object used to access dimensions.
     * @param callback The client callback to receive the processed logo.
     */
    private void processReturnedLogo(String logoUrl, Bitmap image, Resources resources,
            Callback<StatusIconResource> callback) {
        // Scale the logo up to the desired size.
        int logoSizePixels = getSearchEngineLogoSizePixels(resources);
        Bitmap scaledIcon =
                Bitmap.createScaledBitmap(image, getSearchEngineLogoSizePixels(resources),
                        getSearchEngineLogoSizePixels(resources), true);

        Bitmap composedIcon = scaledIcon;
        if (isRoundedSearchEngineLogoEnabled()) {
            int composedSizePixels = getSearchEngineLogoComposedSizePixels(resources);
            if (mRoundedIconGenerator == null) {
                mRoundedIconGenerator = new RoundedIconGenerator(composedSizePixels,
                        composedSizePixels, composedSizePixels, Color.TRANSPARENT, 0);
            }
            int color = (image.getWidth() == 0 || image.getHeight() == 0)
                    ? Color.TRANSPARENT
                    : getMostCommonEdgeColor(image);
            mRoundedIconGenerator.setBackgroundColor(color);

            // Generate a rounded background with no text.
            composedIcon = mRoundedIconGenerator.generateIconForText("");
            Canvas canvas = new Canvas(composedIcon);
            // Draw the logo in the middle of the generated background.
            int dx = (composedSizePixels - logoSizePixels) / 2;
            canvas.drawBitmap(scaledIcon, dx, dx, null);
        }
        // Cache the result icon to reduce future work.
        sCachedComposedBackground = composedIcon;
        sCachedComposedBackgroundLogoUrl = logoUrl;

        callback.onResult(new StatusIconResource(logoUrl, sCachedComposedBackground, 0));
    }

    /**
     * Samples the edges of given bitmap and returns the most common color.
     * @param icon Bitmap to be sampled.
     */
    @VisibleForTesting
    int getMostCommonEdgeColor(Bitmap icon) {
        Map<Integer, Integer> colorCount = new HashMap<>();
        for (int i = 0; i < icon.getWidth(); i++) {
            // top edge
            int color = icon.getPixel(i, 0);
            if (!colorCount.containsKey(color)) colorCount.put(color, 0);
            colorCount.put(color, colorCount.get(color) + 1);

            // bottom edge
            color = icon.getPixel(i, icon.getHeight() - 1);
            if (!colorCount.containsKey(color)) colorCount.put(color, 0);
            colorCount.put(color, colorCount.get(color) + 1);

            // Measure the lateral edges offset by 1 on each side.
            if (i > 0 && i < icon.getWidth() - 1) {
                // left edge
                color = icon.getPixel(0, i);
                if (!colorCount.containsKey(color)) colorCount.put(color, 0);
                colorCount.put(color, colorCount.get(color) + 1);

                // right edge
                color = icon.getPixel(icon.getWidth() - 1, i);
                if (!colorCount.containsKey(color)) colorCount.put(color, 0);
                colorCount.put(color, colorCount.get(color) + 1);
            }
        }

        // Find the most common color out of the map.
        int maxKey = Color.TRANSPARENT;
        int maxVal = -1;
        for (int color : colorCount.keySet()) {
            int count = colorCount.get(color);
            if (count > maxVal) {
                maxKey = color;
                maxVal = count;
            }
        }
        assert maxVal > -1;

        return maxKey;
    }

    /**
     * Records an event to the search engine logo histogram. See {@link Events} and histograms.xml
     * for more details.
     * @param event The {@link Events} to be reported.
     */
    @VisibleForTesting
    void recordEvent(@Events int event) {
        RecordHistogram.recordEnumeratedHistogram(
                "AndroidSearchEngineLogo.Events", event, Events.MAX);
    }

    /** Set the favicon helper for testing. */
    void setFaviconHelperForTesting(FaviconHelper faviconHelper) {
        mFaviconHelper = faviconHelper;
    }

    /** Set the RoundedIconGenerator for testing. */
    void setRoundedIconGeneratorForTesting(RoundedIconGenerator roundedIconGenerator) {
        mRoundedIconGenerator = roundedIconGenerator;
    }

    /** Set the instance for testing. */
    static void setInstanceForTesting(SearchEngineLogoUtils instance) {
        sInstance = instance;
    }

    /** Reset the cache values for testing. */
    static void resetForTesting() {
        sInstance = null;
        sCachedComposedBackground = null;
        sCachedComposedBackgroundLogoUrl = null;
    }
}
