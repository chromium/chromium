// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.intents;

import android.content.Intent;
import android.os.Bundle;
import android.provider.Browser;

import org.chromium.base.IntentUtils;
import org.chromium.chrome.browser.intents.BrowserIntentUtils;
import org.chromium.ui.util.ColorUtils;
import org.chromium.webapk.lib.common.WebApkConstants;

import java.util.HashSet;

/** Utility methods for extracting data from Homescreen-shortcut/WebAPK launch intent. */
public class WebappIntentUtils {
    /**
     * PWA homescreen shortcut intent extras. Used for copying intent extras for
     * {@link WebappActivity} launch intent.
     */
    private static final String[] WEBAPP_INTENT_EXTRAS =
            new String[] {
                WebappConstants.EXTRA_ID,
                WebappConstants.EXTRA_URL,
                WebappConstants.EXTRA_FORCE_NAVIGATION,
                WebappConstants.EXTRA_SOURCE,
                WebappConstants.EXTRA_SCOPE,
                WebappConstants.EXTRA_ICON,
                WebappConstants.EXTRA_VERSION,
                WebappConstants.EXTRA_NAME,
                WebappConstants.EXTRA_SHORT_NAME,
                WebappConstants.EXTRA_DISPLAY_MODE,
                WebappConstants.EXTRA_ORIENTATION,
                WebappConstants.EXTRA_THEME_COLOR,
                WebappConstants.EXTRA_BACKGROUND_COLOR,
                WebappConstants.EXTRA_IS_ICON_GENERATED,
                WebappConstants.EXTRA_IS_ICON_ADAPTIVE,
                WebappConstants.EXTRA_DARK_THEME_COLOR,
                WebappConstants.EXTRA_DARK_BACKGROUND_COLOR,
                BrowserIntentUtils.EXTRA_STARTUP_UPTIME_MS,
                BrowserIntentUtils.EXTRA_STARTUP_REALTIME_MS
            };

    /**
     * WebAPK intent extras. Used for copying intent extras for {@link WebappActivity} launch
     * intent.
     */
    private static final String[] WEBAPK_INTENT_EXTRAS =
            new String[] {
                WebappConstants.EXTRA_ID,
                WebappConstants.EXTRA_URL,
                WebappConstants.EXTRA_FORCE_NAVIGATION,
                WebappConstants.EXTRA_SOURCE,
                WebApkConstants.EXTRA_WEBAPK_PACKAGE_NAME,
                WebApkConstants.EXTRA_SPLASH_PROVIDED_BY_WEBAPK,
                WebApkConstants.EXTRA_WEBAPK_LAUNCH_TIME,
                WebApkConstants.EXTRA_NEW_STYLE_SPLASH_SHOWN_TIME,
                WebApkConstants.EXTRA_WEBAPK_SELECTED_SHARE_TARGET_ACTIVITY_CLASS_NAME,
                Intent.EXTRA_SUBJECT,
                Intent.EXTRA_TEXT,
                Intent.EXTRA_STREAM,
                Browser.EXTRA_APPLICATION_ID,
                BrowserIntentUtils.EXTRA_STARTUP_UPTIME_MS,
                BrowserIntentUtils.EXTRA_STARTUP_REALTIME_MS
            };

    /**
     * Converts color from signed Integer where an unspecified color is represented as null to
     * to unsigned long where an unspecified color is represented as
     * {@link ColorUtils.INVALID_COLOR}.
     */
    public static long colorFromIntegerColor(Integer color) {
        if (color != null) {
            return color.intValue();
        }
        return ColorUtils.INVALID_COLOR;
    }

    public static boolean isLongColorValid(long longColor) {
        return (longColor != ColorUtils.INVALID_COLOR);
    }

    /**
     * Converts color from unsigned long where an unspecified color is represented as
     * {@link ColorUtils.INVALID_COLOR} to a signed Integer where an
     * unspecified color is represented as null.
     */
    public static Integer colorFromLongColor(long longColor) {
        return isLongColorValid(longColor) ? Integer.valueOf((int) longColor) : null;
    }

    /** Extracts id from homescreen shortcut intent. */
    public static String getIdForHomescreenShortcut(Intent intent) {
        return IntentUtils.safeGetStringExtra(intent, WebappConstants.EXTRA_ID);
    }

    /** Generates id for the passed-in WebAPK package name. */
    public static String getIdForWebApkPackage(String packageName) {
        return WebApkConstants.WEBAPK_ID_PREFIX + packageName;
    }

    public static String getUrl(Intent intent) {
        return IntentUtils.safeGetStringExtra(intent, WebApkConstants.EXTRA_URL);
    }

    public static String getWebApkPackageName(Intent intent) {
        return IntentUtils.safeGetStringExtra(intent, WebApkConstants.EXTRA_WEBAPK_PACKAGE_NAME);
    }

    /** Returns the WebAPK's shell launch timestamp associated with the passed-in intent, or -1. */
    public static long getWebApkShellLaunchTime(Intent intent) {
        return intent.getLongExtra(WebApkConstants.EXTRA_WEBAPK_LAUNCH_TIME, -1);
    }

    /**
     * Returns the timestamp when the WebAPK shell showed the splash screen. Returns -1 if the
     * WebAPK shell did not show the splash screen.
     */
    public static long getNewStyleWebApkSplashShownTime(Intent intent) {
        return intent.getLongExtra(WebApkConstants.EXTRA_NEW_STYLE_SPLASH_SHOWN_TIME, -1);
    }

    public static void copyWebappLaunchIntentExtras(Intent fromIntent, Intent toIntent) {
        copyIntentExtras(fromIntent, toIntent, WEBAPP_INTENT_EXTRAS);
    }

    public static void copyWebApkLaunchIntentExtras(Intent fromIntent, Intent toIntent) {
        copyIntentExtras(fromIntent, toIntent, WEBAPK_INTENT_EXTRAS);
    }

    /**
     * Copies intent extras.
     * @param fromIntent Intent to copy extras from.
     * @param toIntent Intent to copy extras to.
     * @param key Keys to be copied.
     */
    private static void copyIntentExtras(Intent fromIntent, Intent toIntent, String[] keys) {
        Bundle extras = fromIntent.getExtras();
        if (extras == null) return;

        // Make a copy of all of the intent extras and remove the ones we do not want to copy to
        // avoid dealing with the types of the extras.

        // Make a copy of Intent#keySet() to avoid modifying |fromIntent|.
        HashSet<String> extraKeysToRemove = new HashSet<>(extras.keySet());
        for (String key : keys) {
            extraKeysToRemove.remove(key);
        }
        for (String key : extraKeysToRemove) {
            extras.remove(key);
        }
        toIntent.putExtras(extras);
    }
}
