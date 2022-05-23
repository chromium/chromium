// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_zoom;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.MathUtils;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.content_public.browser.ContentFeatureList;
import org.chromium.content_public.browser.HostZoomMap;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.Arrays;

/**
 * Internal Mediator for the page zoom feature. Created by the |PageZoomCoordinator|, and should
 * not be accessed outside the component.
 */
public class PageZoomMediator {
    /**
     * Available zoom levels as they would be presented to a user. These match the currently
     * used levels on Chrome Desktop. See: components/zoom/page_zoom_constants.cc
     */
    private static final double[] AVAILABLE_ZOOM_LEVELS = new double[] {0.25, 0.33, 0.50, 0.67,
            0.75, 0.80, 0.90, 1.00, 1.10, 1.25, 1.33, 1.50, 1.75, 2.00, 2.50, 3.00, 4.00, 5.00};

    /**
     * Available zoom factors that correspond to the zoom levels above. These numbers are used
     * internally to give the above zoom levels and are not presented to the user. These become
     * the exponent that |kTextSizeMultiplierRatio| = 1.2 is raised to for the above numbers,
     * e.g. 1.2^-7.6 = 0.25, or 1.2^3.8 = 2.0. See: third_party/blink/common/page/page_zoom.cc
     */
    private static final double[] AVAILABLE_ZOOM_FACTORS = new double[] {-7.60, -6.08, -3.80, -2.20,
            -1.58, -1.22, -0.58, 0.00, 0.52, 1.22, 1.56, 2.22, 3.07, 3.80, 5.03, 6.03, 7.60, 8.83};

    // Default index for zoom factor, set to be 100%.
    private static final int DEFAULT_ZOOM_FACTOR_INDEX = 7;

    // Current zoom factor set by the user.
    private int mZoomIndex = DEFAULT_ZOOM_FACTOR_INDEX;

    private final PropertyModel mModel;
    private WebContents mWebContents;

    public PageZoomMediator(PropertyModel model) {
        mModel = model;

        mModel.set(PageZoomProperties.DECREASE_ZOOM_CALLBACK, this::handleDecreaseClicked);
        mModel.set(PageZoomProperties.INCREASE_ZOOM_CALLBACK, this::handleIncreaseClicked);
        mModel.set(PageZoomProperties.MAXIMUM_ZOOM, AVAILABLE_ZOOM_FACTORS.length - 1);
    }

    /**
     * Returns whether the AppMenu item for Zoom should be displayed. It will be displayed if
     * any of the following conditions are met:
     *
     *    - User has enabled the "Show Page Zoom" setting in Chrome Accessibility Settings
     *    - User has set a default zoom other than 100% in Chrome Accessibility Settings
     *    - User has changed the Android OS Font Size setting
     *
     * @return boolean
     */
    public static boolean shouldShowMenuItem() {
        // Never show the menu item if the content feature is disabled.
        if (!ContentFeatureList.isEnabled(ContentFeatureList.ACCESSIBILITY_PAGE_ZOOM)) {
            return false;
        }

        // Always show the menu item if the user has set this in Accessibility Settings.
        if (SharedPreferencesManager.getInstance().readBoolean(
                    ChromePreferenceKeys.PAGE_ZOOM_ALWAYS_SHOW_MENU_ITEM, false)) {
            return true;
        }

        // The default (float) |fontScale| is 1, the default page zoom is 1.
        boolean nonDefaultSystemFontSize = MathUtils.areFloatsEqual(
                ContextUtils.getApplicationContext().getResources().getConfiguration().fontScale,
                1f);

        boolean nonDefaultDefaultPageZoom = MathUtils.areFloatsEqual(
                SharedPreferencesManager.getInstance().readFloat(
                        ChromePreferenceKeys.PAGE_ZOOM_DEFAULT_ZOOM_SETTING, 1.0f),
                1f);

        return nonDefaultSystemFontSize || nonDefaultDefaultPageZoom;
    }

    /**
     * Set the web contents that should be controlled by this instance.
     * @param webContents   The WebContents this instance should control.
     */
    public void setWebContents(WebContents webContents) {
        mWebContents = webContents;
        mZoomIndex = Arrays.binarySearch(AVAILABLE_ZOOM_FACTORS, getZoomLevel(mWebContents));
        updateState();
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    void handleDecreaseClicked(Void unused) {
        // Check if we are already at the minimum zoom.
        if (mZoomIndex <= 0) return;

        --mZoomIndex;
        updateState();
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    void handleIncreaseClicked(Void unused) {
        // Check if we are already at the maximum zoom.
        if (mZoomIndex >= AVAILABLE_ZOOM_FACTORS.length - 1) return;

        ++mZoomIndex;
        updateState();
    }

    private void updateState() {
        setZoomLevel(mWebContents, AVAILABLE_ZOOM_FACTORS[mZoomIndex]);
        mModel.set(PageZoomProperties.CURRENT_ZOOM, mZoomIndex);
    }

    // Pass-through methods to HostZoomMap, which has static methods to call through JNI.
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    void setZoomLevel(@NonNull WebContents webContents, double newZoomLevel) {
        HostZoomMap.setZoomLevel(webContents, newZoomLevel);
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    double getZoomLevel(@NonNull WebContents webContents) {
        return HostZoomMap.getZoomLevel(webContents);
    }
}
