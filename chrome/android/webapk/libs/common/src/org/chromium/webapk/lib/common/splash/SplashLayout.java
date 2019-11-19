// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.lib.common.splash;

import android.content.Context;
import android.content.res.Resources;
import android.content.res.Resources.NotFoundException;
import android.graphics.Bitmap;
import android.graphics.drawable.Icon;
import android.os.Build;
import android.util.DisplayMetrics;
import android.view.LayoutInflater;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

/**
 * Contains utility methods for drawing splash screen. The methods are applicable for both home
 * screen shortcuts and WebAPKs.
 */
public class SplashLayout {
    public static int getDefaultBackgroundColor(Context appContext) {
        return getColorCompatibility(appContext.getResources(), R.color.webapp_default_bg);
    }

    /**
     * Selects the splash screen layout based on:
     * - Whether the icon is appropriate to display on the splash screen.
     * - The icon size.
     */
    public static int selectLayout(Resources resources, Bitmap icon, boolean wasIconGenerated) {
        if (icon == null || wasIconGenerated) {
            return R.layout.webapp_splash_screen_no_icon;
        }
        DisplayMetrics metrics = resources.getDisplayMetrics();
        int smallestEdge = Math.min(icon.getScaledWidth(metrics), icon.getScaledHeight(metrics));
        int minimumSizeThreshold =
                resources.getDimensionPixelSize(R.dimen.webapp_splash_image_size_minimum);
        if (smallestEdge < minimumSizeThreshold) {
            return R.layout.webapp_splash_screen_no_icon;
        }
        return R.layout.webapp_splash_screen_large;
    }

    /**
     * @see android.content.res.Resources#getColor(int id).
     */
    @SuppressWarnings("deprecation")
    public static int getColorCompatibility(Resources res, int id) throws NotFoundException {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            return res.getColor(id, null);
        } else {
            return res.getColor(id);
        }
    }

    /** Builds splash screen and attaches it to the parent view. */
    public static void createLayout(Context appContext, ViewGroup parentView, Bitmap icon,
            boolean isIconAdaptive, boolean isIconGenerated, String text,
            boolean useLightTextColor) {
        int layoutId = selectLayout(appContext.getResources(), icon, isIconGenerated);
        ViewGroup layout =
                (ViewGroup) LayoutInflater.from(appContext).inflate(layoutId, parentView, true);

        TextView appNameView = (TextView) layout.findViewById(R.id.webapp_splash_screen_name);
        appNameView.setText(text);
        if (useLightTextColor) {
            appNameView.setTextColor(getColorCompatibility(
                    appContext.getResources(), R.color.webapp_splash_title_light));
        }

        ImageView splashIconView = (ImageView) layout.findViewById(R.id.webapp_splash_screen_icon);
        if (splashIconView == null) return;

        // Adaptive icons should only be present on Android O.
        if (isIconAdaptive && Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            splashIconView.setImageIcon(Icon.createWithAdaptiveBitmap(icon));
        } else {
            splashIconView.setImageBitmap(icon);
        }
    }
}
