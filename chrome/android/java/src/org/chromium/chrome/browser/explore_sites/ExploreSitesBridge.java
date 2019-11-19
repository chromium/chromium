// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.explore_sites;

import android.content.Context;
import android.graphics.Bitmap;
import android.util.DisplayMetrics;
import android.view.WindowManager;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.profiles.Profile;

import java.util.ArrayList;
import java.util.List;

/**
 * The model and controller for a group of explore options.
 */
@JNINamespace("explore_sites")
public class ExploreSitesBridge {
    private static final String TAG = "ExploreSitesBridge";

    private static List<ExploreSitesCategory> sCatalogForTesting;
    public static void setCatalogForTesting(List<ExploreSitesCategory> catalog) {
        sCatalogForTesting = catalog;
    }

    /**
     * @Deprecated Please use getCatalog instead.
     *
     * Fetches the catalog data from disk for Explore surfaces.
     *
     * Callback will be called with |null| if an error occurred.
     */
    @Deprecated
    public static void getEspCatalog(
            Profile profile, Callback<List<ExploreSitesCategory>> callback) {
        if (sCatalogForTesting != null) {
            callback.onResult(sCatalogForTesting);
            return;
        }

        List<ExploreSitesCategory> result = new ArrayList<>();
        ExploreSitesBridgeJni.get().getEspCatalog(profile, result, callback);
    }

    /**
     * Retrieves the catalog data for Explore surfaces by attempting to retrieve the data from disk.
     *
     * If the catalog is not available on the disk, then loads the catalog from the network onto the
     * disk and returns the new catalog from the disk.
     *
     * Use ExploreSitesBridge.isValidCatalog() to check for failure, as the callback can have a null
     * catalog.
     *
     * @param profile - Profile associated with this update.
     * @param source - int identifying source from ExploreSitesCatalogUpdateRequestSource.
     * @param callback - method to call with resulting catalog.
     *
     */
    public static void getCatalog(Profile profile,
            @ExploreSitesCatalogUpdateRequestSource int source,
            Callback<List<ExploreSitesCategory>> callback) {
        if (sCatalogForTesting != null) {
            callback.onResult(sCatalogForTesting);
            return;
        }

        List<ExploreSitesCategory> result = new ArrayList<>();
        ExploreSitesBridgeJni.get().getCatalog(profile, source, result, callback);
    }

    /**
     * Check if a catalog is valid.
     */
    public static boolean isValidCatalog(List<ExploreSitesCategory> catalog) {
        return catalog != null && !catalog.isEmpty();
    }

    /**
     * Update the catalog from network. Takes care of updating histograms for
     * ExploreSites.NTPLoadingCatalogFromNetwork and ExploreSites.CatalogUpdateRequestSource.
     */
    public static void initializeCatalog(
            Profile profile, @ExploreSitesCatalogUpdateRequestSource int source) {
        ExploreSitesBridgeJni.get().initializeCatalog(profile, source);
    }

    public static void getSiteImage(Profile profile, int siteID, Callback<Bitmap> callback) {
        if (sCatalogForTesting != null) {
            callback.onResult(null);
            return;
        }
        ExploreSitesBridgeJni.get().getIcon(profile, siteID, callback);
    }

    /**
     * Returns a Bitmap representing a summary of the sites available in the catalog for a specific
     * category.
     */
    public static void getCategoryImage(
            Profile profile, int categoryID, int pixelSize, Callback<Bitmap> callback) {
        if (sCatalogForTesting != null) {
            callback.onResult(null);
            return;
        }
        ExploreSitesBridgeJni.get().getCategoryImage(profile, categoryID, pixelSize, callback);
    }

    /**
     * Returns a Bitmap representing a summary of the sites available in the catalog.
     */
    public static void getSummaryImage(Profile profile, int pixelSize, Callback<Bitmap> callback) {
        if (sCatalogForTesting != null) {
            callback.onResult(null);
            return;
        }
        ExploreSitesBridgeJni.get().getSummaryImage(profile, pixelSize, callback);
    }

    /**
     * Causes a network request for updating the catalog.
     */
    public static void updateCatalogFromNetwork(
            Profile profile, boolean isImmediateFetch, Callback<Boolean> finishedCallback) {
        ExploreSitesBridgeJni.get().updateCatalogFromNetwork(
                profile, isImmediateFetch, finishedCallback);
    }

    /**
     * Adds a site to the blacklist when the user chooses "remove" from the long press menu.
     */
    public static void blacklistSite(Profile profile, String url) {
        ExploreSitesBridgeJni.get().blacklistSite(profile, url);
    }

    /**
     * Records that a site has been clicked.
     */
    public static void recordClick(
            Profile profile, String url, @ExploreSitesCategory.CategoryType int type) {
        ExploreSitesBridgeJni.get().recordClick(profile, url, type);
    }

    /**
     * Gets the current Finch variation that is configured by flag or experiment.
     */
    @ExploreSitesVariation
    public static int getVariation() {
        return ExploreSitesBridgeJni.get().getVariation();
    }

    /**
     * Gets the current Finch variation for last MostLikely icon that is configured by flag or
     * experiment.
     */
    @MostLikelyVariation
    public static int getIconVariation() {
        return ExploreSitesBridgeJni.get().getIconVariation();
    }

    /**
     * Gets the current Finch variation for dense that is configured by flag or experiment.
     * */
    @DenseVariation
    public static int getDenseVariation() {
        return ExploreSitesBridgeJni.get().getDenseVariation();
    }

    public static boolean isEnabled(@ExploreSitesVariation int variation) {
        return variation == ExploreSitesVariation.ENABLED
                || variation == ExploreSitesVariation.PERSONALIZED
                || variation == ExploreSitesVariation.MOST_LIKELY;
    }

    public static boolean isExperimental(@ExploreSitesVariation int variation) {
        return variation == ExploreSitesVariation.EXPERIMENT;
    }

    public static boolean isDense(@DenseVariation int variation) {
        return variation != DenseVariation.ORIGINAL;
    }

    public static boolean isIntegratedWithMostLikely(@ExploreSitesVariation int variation) {
        return variation == ExploreSitesVariation.MOST_LIKELY;
    }

    /**
     * Increments the ntp_shown_count for a particular category.
     * @param categoryId the row id of the category to increment show count for.
     */
    public static void incrementNtpShownCount(Profile profile, int categoryId) {
        ExploreSitesBridgeJni.get().incrementNtpShownCount(profile, categoryId);
    }

    @CalledByNative
    static void scheduleDailyTask() {
        ExploreSitesBackgroundTask.schedule(false /* updateCurrent */);
    }

    /**
     * Returns the scale factor on this device.
     */
    @CalledByNative
    static float getScaleFactorFromDevice() {
        // Get DeviceMetrics from context.
        DisplayMetrics metrics = new DisplayMetrics();
        ((WindowManager) ContextUtils.getApplicationContext().getSystemService(
                 Context.WINDOW_SERVICE))
                .getDefaultDisplay()
                .getMetrics(metrics);
        // Get density and return it.
        return metrics.density;
    }

    @NativeMethods
    interface Natives {
        int getVariation();
        int getIconVariation();
        int getDenseVariation();
        void getEspCatalog(Profile profile, List<ExploreSitesCategory> result,
                Callback<List<ExploreSitesCategory>> callback);
        void getIcon(Profile profile, int siteID, Callback<Bitmap> callback);
        void updateCatalogFromNetwork(
                Profile profile, boolean isImmediateFetch, Callback<Boolean> callback);
        void getCategoryImage(
                Profile profile, int categoryID, int pixelSize, Callback<Bitmap> callback);
        void getSummaryImage(Profile profile, int pixelSize, Callback<Bitmap> callback);
        void blacklistSite(Profile profile, String url);
        void recordClick(Profile profile, String url, int type);
        void incrementNtpShownCount(Profile profile, int categoryId);
        void getCatalog(Profile profile, int source, List<ExploreSitesCategory> result,
                Callback<List<ExploreSitesCategory>> callback);
        void initializeCatalog(Profile profile, int source);
    }
}
