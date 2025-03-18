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

/** Utility methods for calculating the dimensions of a Custom Tab. */
public class CustomTabDimensionUtils {
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
}
