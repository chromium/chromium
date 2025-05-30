// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features;

import android.app.Activity;
import android.graphics.Insets;
import android.graphics.Point;
import android.graphics.Rect;
import android.os.Build;
import android.view.Display;
import android.view.WindowInsets;

import androidx.annotation.Px;
import androidx.annotation.RequiresApi;

import org.chromium.base.MathUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;

/** Utility methods for calculating the dimensions of a Custom Tab. */
@NullMarked
public class CustomTabDimensionUtils {
    private static final int WINDOW_WIDTH_EXPANDED_CUTOFF_DP = 840;
    private static final float MINIMAL_WIDTH_RATIO_EXPANDED = 0.33f;
    private static final float MINIMAL_WIDTH_RATIO_MEDIUM = 0.5f;

    public static int getDisplayWidth(Activity activity) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            return getDisplayWidthR(activity);
        }
        return getDisplayWidthPreR(activity);
    }

    @RequiresApi(Build.VERSION_CODES.R)
    @Px
    public static int getDisplayWidthR(Activity activity) {
        Insets navbarInsets =
                activity.getWindowManager()
                        .getCurrentWindowMetrics()
                        .getWindowInsets()
                        .getInsets(
                                WindowInsets.Type.navigationBars()
                                        | WindowInsets.Type.displayCutout());
        int navbarWidth = navbarInsets.left + navbarInsets.right;
        Rect windowBounds = activity.getWindowManager().getCurrentWindowMetrics().getBounds();
        return windowBounds.width() - navbarWidth;
    }

    @Px
    public static int getDisplayWidthPreR(Activity activity) {
        Display display = activity.getWindowManager().getDefaultDisplay();
        Point size = new Point();
        display.getSize(size);
        return size.x;
    }

    /**
     * Initial width of the CCT based on the screen metrics and intent data.
     *
     * @param activity The {@link Activity} for the screen metrics.
     * @param intentDataProvider The {@link BrowserServicesIntentDataProvider} for the intent data.
     * @return The initial width of the CCT.
     */
    @Px
    public static int getInitialWidth(
            Activity activity, BrowserServicesIntentDataProvider intentDataProvider) {
        int unclampedWidth = intentDataProvider.getInitialActivityWidth();
        int displayWidth = getDisplayWidth(activity);

        if (unclampedWidth <= 0) {
            return getDisplayWidth(activity);
        }

        int displayWidthDp =
                (int) (displayWidth / activity.getResources().getDisplayMetrics().density);
        return getInitialWidth(unclampedWidth, displayWidth, displayWidthDp);
    }

    /**
     * Initial width of the CCT based on the width values.
     *
     * @param unclampedWidth Unclamped width of the CCT.
     * @param displayWidth Display width in pixels.
     * @param displayWidthDp Display width in dps.
     * @return The initial width of the CCT.
     */
    @Px
    public static int getInitialWidth(
            @Px int unclampedWidth, @Px int displayWidth, int displayWidthDp) {
        float minWidthRatio =
                displayWidthDp < WINDOW_WIDTH_EXPANDED_CUTOFF_DP
                        ? MINIMAL_WIDTH_RATIO_MEDIUM
                        : MINIMAL_WIDTH_RATIO_EXPANDED;
        return MathUtils.clamp(unclampedWidth, displayWidth, (int) (displayWidth * minWidthRatio));
    }
}
