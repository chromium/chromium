// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import static org.chromium.components.embedder_support.application.ClassLoaderContextWrapperFactory.getOriginalApplicationContext;

import android.content.Context;
import android.content.res.Configuration;
import android.content.res.TypedArray;
import android.os.Build;

import androidx.core.graphics.ColorUtils;

import org.chromium.base.Log;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** This class provides the utility methods for dark mode. */
public class DarkModeHelper {
    private static final String TAG = "DarkModeHelper";

    @Retention(RetentionPolicy.SOURCE)
    public @interface LightTheme {
        int LIGHT_THEME_UNDEFINED = 0;
        int LIGHT_THEME_FALSE = 1;
        int LIGHT_THEME_TRUE = 2;
        int LIGHT_THEME_COUNT = 3;
    }

    @Retention(RetentionPolicy.SOURCE)
    public @interface TextLuminance {
        int TEXT_LUMINACE_UNDEFINED = 0;
        int TEXT_LUMINACE_LIGHT = 1;
        int TEXT_LUMINACE_DARK = 2;
        int TEXT_LUMINACE_COUNT = 3;
    }

    @Retention(RetentionPolicy.SOURCE)
    public @interface NightMode {
        int NGITH_MODE_UNDEFINED = 0;
        int NIGHT_MODE_ON = 1;
        int NIGHT_MODE_OFF = 2;
        int NIGHT_MODE_COUNT = 3;
    }

    private static Integer sLightThemeForTesting;

    @NightMode
    public static int getNightMode(Context context) {
        int nightMode =
                getOriginalApplicationContext(context).getResources().getConfiguration().uiMode
                        & Configuration.UI_MODE_NIGHT_MASK;
        switch (nightMode) {
            case Configuration.UI_MODE_NIGHT_NO:
                return NightMode.NIGHT_MODE_OFF;
            case Configuration.UI_MODE_NIGHT_YES:
                return NightMode.NIGHT_MODE_ON;
            case Configuration.UI_MODE_NIGHT_UNDEFINED:
            default:
                return NightMode.NGITH_MODE_UNDEFINED;
        }
    }

    // must use getIdentifier to access resources from another app
    @SuppressWarnings("DiscouragedApi")
    @LightTheme
    public static int getLightTheme(Context context) {
        if (sLightThemeForTesting != null) return sLightThemeForTesting;
        int lightTheme = LightTheme.LIGHT_THEME_UNDEFINED;
        try {
            int resId = android.R.attr.isLightTheme;
            if (Build.VERSION.SDK_INT < Build.VERSION_CODES.Q) {
                // android.R.attr.isLightTheme is added in Q, for pre-Q platform, WebView
                // checks if app has isLightTheme attr which could be added by Android X
                // and wasn't stripped out.
                resId =
                        getOriginalApplicationContext(context)
                                .getResources()
                                .getIdentifier(
                                        "isLightTheme",
                                        "attr",
                                        context.getApplicationContext().getPackageName());
                if (resId == 0) return lightTheme;
            }
            TypedArray a =
                    getOriginalApplicationContext(context)
                            .getTheme()
                            .obtainStyledAttributes(new int[] {resId});
            // TODO: use try-with-resources once minSdkVersion>=31 instead of recycle
            try {
                if (a.hasValue(0)) {
                    lightTheme =
                            a.getBoolean(0, true)
                                    ? LightTheme.LIGHT_THEME_TRUE
                                    : LightTheme.LIGHT_THEME_FALSE;
                }
            } finally {
                a.recycle();
            }
        } catch (RuntimeException e) {
            // The AssetManager may have been shut down, possibly due to the WebView outliving the
            // Activity it was associated with, but this just throws a generic RuntimeException.
            // Check the message to be sure.
            if ("AssetManager has been destroyed".equals(e.getMessage())) {
                // just fall through so we return the default
            } else {
                // rethrow if the message doesn't match
                throw e;
            }
        }
        return lightTheme;
    }

    public static void setsLightThemeForTesting(@LightTheme int lightThemeForTesting) {
        sLightThemeForTesting = Integer.valueOf(lightThemeForTesting);
    }

    @TextLuminance
    public static int getPrimaryTextLuminace(Context context) {
        int textColor = TextLuminance.TEXT_LUMINACE_UNDEFINED;
        TypedArray a =
                getOriginalApplicationContext(context)
                        .getTheme()
                        .obtainStyledAttributes(new int[] {android.R.attr.textColorPrimary});
        if (a.hasValue(0)) {
            try {
                textColor =
                        ColorUtils.calculateLuminance(a.getColor(0, 0)) < 0.5
                                ? TextLuminance.TEXT_LUMINACE_DARK
                                : TextLuminance.TEXT_LUMINACE_LIGHT;
            } catch (UnsupportedOperationException e) {
                Log.e(TAG, "Wrong color format", e);
            }
        }
        a.recycle();
        return textColor;
    }
}
