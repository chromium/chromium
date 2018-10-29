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
import org.chromium.chrome.browser.profiles.Profile;

import java.util.ArrayList;
import java.util.List;

/**
 * The model and controller for a group of explore options.
 */
@JNINamespace("explore_sites")
public class ExploreSitesBridge {
    private static final String TAG = "ExploreSitesBridge";

    /**
     * Fetches the catalog data for Explore page.
     *
     * Callback will be called with |null| if an error occurred.
     */
    public static void getEspCatalog(
            Profile profile, Callback<List<ExploreSitesCategory>> callback) {
        List<ExploreSitesCategory> result = new ArrayList<>();
        nativeGetEspCatalog(profile, result, callback);
    }

    public static void getSiteImage(Profile profile, int siteID, Callback<Bitmap> callback) {
        nativeGetIcon(profile, siteID, callback);
    }

    public static void getCategoryImage(
            Profile profile, int categoryID, int pixelSize, Callback<Bitmap> callback) {
        nativeGetCategoryImage(profile, categoryID, pixelSize, callback);
    }

    /**
     * Causes a network request for updating the catalog.
     */
    public static void updateCatalogFromNetwork(
            Profile profile, boolean isImmediateFetch, Callback<Boolean> finishedCallback) {
        nativeUpdateCatalogFromNetwork(profile, isImmediateFetch, finishedCallback);
    }
    /**
     * Adds a site to the blacklist when the user chooses "remove" from the long press menu.
     */
    public static void blacklistSite(Profile profile, String url) {
        nativeBlacklistSite(profile, url);
    }

    /**
     * Gets the current Finch variation that is configured by flag or experiment.
     */
    @ExploreSitesVariation
    public static int getVariation() {
        return nativeGetVariation();
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

    static native int nativeGetVariation();
    private static native void nativeGetEspCatalog(Profile profile,
            List<ExploreSitesCategory> result, Callback<List<ExploreSitesCategory>> callback);

    private static native void nativeGetIcon(
            Profile profile, int siteID, Callback<Bitmap> callback);

    private static native void nativeUpdateCatalogFromNetwork(
            Profile profile, boolean isImmediateFetch, Callback<Boolean> callback);

    private static native void nativeGetCategoryImage(
            Profile profile, int categoryID, int pixelSize, Callback<Bitmap> callback);

    private static native void nativeBlacklistSite(Profile profile, String url);
}
