// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.lib.common.splash;

import android.content.Context;
import android.content.res.Resources;
import android.content.res.Resources.NotFoundException;
import android.graphics.Bitmap;
import android.os.Build;
import android.support.annotation.IntDef;
import android.util.DisplayMetrics;
import android.view.LayoutInflater;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Contains utility methods for drawing splash screen. The methods are applicable for both home
 * screen shortcuts and WebAPKs.
 */
public class SplashLayout {
    @IntDef({IconClassification.INVALID, IconClassification.SMALL, IconClassification.LARGE})
    @Retention(RetentionPolicy.SOURCE)
    public @interface IconClassification {
        int INVALID = 0;
        int SMALL = 1;
        int LARGE = 2;
    }

    /**
     * Classifies the icon based on:
     * - Whether it is appropriate to display on the splash screen.
     * - The icon size.
     * Returns {@link IconClassification.INVALID} if the icon is inappropriate to display on the
     * splash screen.
     */
    public static @IconClassification int classifyIcon(
            Resources resources, Bitmap icon, boolean wasIconGenerated) {
        if (icon == null || wasIconGenerated) {
            return IconClassification.INVALID;
        }
        DisplayMetrics metrics = resources.getDisplayMetrics();
        int smallestEdge = Math.min(icon.getScaledWidth(metrics), icon.getScaledHeight(metrics));
        int minimumSizeThreshold =
                resources.getDimensionPixelSize(R.dimen.webapp_splash_image_size_minimum);
        if (smallestEdge < minimumSizeThreshold) {
            return IconClassification.INVALID;
        }
        int bigThreshold =
                resources.getDimensionPixelSize(R.dimen.webapp_splash_image_size_threshold);
        return (smallestEdge <= bigThreshold) ? IconClassification.SMALL : IconClassification.LARGE;
    }

    /** Returns the layout for the splash screen given the classification of the splash icon. */
    private static int selectLayoutFromIconClassification(
            @IconClassification int iconClassification) {
        switch (iconClassification) {
            case IconClassification.INVALID:
                return R.layout.webapp_splash_screen_no_icon;
            case IconClassification.SMALL:
                return R.layout.webapp_splash_screen_small;
            case IconClassification.LARGE:
            default:
                return R.layout.webapp_splash_screen_large;
        }
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
            @IconClassification int iconClassification, String text, boolean useLightTextColor) {
        int layoutId = selectLayoutFromIconClassification(iconClassification);
        ViewGroup layout =
                (ViewGroup) LayoutInflater.from(appContext).inflate(layoutId, parentView, true);

        TextView appNameView = (TextView) layout.findViewById(R.id.webapp_splash_screen_name);
        appNameView.setText(text);
        if (useLightTextColor) {
            appNameView.setTextColor(getColorCompatibility(
                    appContext.getResources(), R.color.webapp_splash_title_light));
        }

        ImageView splashIconView = (ImageView) layout.findViewById(R.id.webapp_splash_screen_icon);
        if (splashIconView != null) splashIconView.setImageBitmap(icon);
    }
}
