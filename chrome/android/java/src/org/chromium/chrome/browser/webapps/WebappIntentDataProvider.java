// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.content.Intent;
import android.graphics.Color;
import android.text.TextUtils;

import androidx.annotation.Nullable;
import androidx.browser.customtabs.CustomTabsIntent;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.ShortcutSource;
import org.chromium.chrome.browser.browserservices.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.util.IntentUtils;
import org.chromium.content_public.common.ScreenOrientationValues;
import org.chromium.webapk.lib.common.splash.SplashLayout;

/**
 * Stores info about a web app.
 */
public class WebappIntentDataProvider extends BrowserServicesIntentDataProvider {
    private static final String TAG = "WebappInfo";

    private int mToolbarColor;
    private WebappExtras mWebappExtras;

    public static WebappIntentDataProvider createEmpty() {
        return new WebappIntentDataProvider(getDefaultToolbarColor(), WebappExtras.createEmpty());
    }

    /**
     * Returns the toolbar color to use if a custom color is not specified by the webapp.
     */
    public static int getDefaultToolbarColor() {
        return Color.WHITE;
    }

    /**
     * Converts color from signed Integer where an unspecified color is represented as null to
     * to unsigned long where an unspecified color is represented as
     * {@link ShortcutHelper.MANIFEST_COLOR_INVALID_OR_MISSING}.
     */
    public static long colorFromIntegerColor(Integer color) {
        if (color != null) {
            return color.intValue();
        }
        return ShortcutHelper.MANIFEST_COLOR_INVALID_OR_MISSING;
    }

    public static boolean isLongColorValid(long longColor) {
        return (longColor != ShortcutHelper.MANIFEST_COLOR_INVALID_OR_MISSING);
    }

    /**
     * Converts color from unsigned long where an unspecified color is represented as
     * {@link ShortcutHelper.MANIFEST_COLOR_INVALID_OR_MISSING} to a signed Integer where an
     * unspecified color is represented as null.
     */
    public static Integer colorFromLongColor(long longColor) {
        return isLongColorValid(longColor) ? Integer.valueOf((int) longColor) : null;
    }

    public static String idFromIntent(Intent intent) {
        return IntentUtils.safeGetStringExtra(intent, ShortcutHelper.EXTRA_ID);
    }

    private static int sourceFromIntent(Intent intent) {
        int source = IntentUtils.safeGetIntExtra(
                intent, ShortcutHelper.EXTRA_SOURCE, ShortcutSource.UNKNOWN);
        if (source >= ShortcutSource.COUNT) {
            source = ShortcutSource.UNKNOWN;
        }
        return source;
    }

    private static String titleFromIntent(Intent intent) {
        // The reference to title has been kept for reasons of backward compatibility. For intents
        // and shortcuts which were created before we utilized the concept of name and shortName,
        // we set the name and shortName to be the title.
        String title = IntentUtils.safeGetStringExtra(intent, ShortcutHelper.EXTRA_TITLE);
        return title == null ? "" : title;
    }

    private static String nameFromIntent(Intent intent) {
        String name = IntentUtils.safeGetStringExtra(intent, ShortcutHelper.EXTRA_NAME);
        return name == null ? titleFromIntent(intent) : name;
    }

    private static String shortNameFromIntent(Intent intent) {
        String shortName = IntentUtils.safeGetStringExtra(intent, ShortcutHelper.EXTRA_SHORT_NAME);
        return shortName == null ? titleFromIntent(intent) : shortName;
    }

    /**
     * Construct a WebappIntentDataProvider.
     * @param intent Intent containing info about the app.
     */
    public static WebappIntentDataProvider create(Intent intent) {
        String id = idFromIntent(intent);
        String url = IntentUtils.safeGetStringExtra(intent, ShortcutHelper.EXTRA_URL);
        if (id == null || url == null) {
            Log.e(TAG, "Incomplete data provided: " + id + ", " + url);
            return null;
        }

        long themeColor = IntentUtils.safeGetLongExtra(intent, ShortcutHelper.EXTRA_THEME_COLOR,
                ShortcutHelper.MANIFEST_COLOR_INVALID_OR_MISSING);
        boolean hasValidToolbarColor = isLongColorValid(themeColor);
        int toolbarColor = hasValidToolbarColor ? (int) themeColor : getDefaultToolbarColor();

        String icon = IntentUtils.safeGetStringExtra(intent, ShortcutHelper.EXTRA_ICON);

        String scope = IntentUtils.safeGetStringExtra(intent, ShortcutHelper.EXTRA_SCOPE);
        if (TextUtils.isEmpty(scope)) {
            scope = ShortcutHelper.getScopeFromUrl(url);
        }

        @WebDisplayMode
        int displayMode = IntentUtils.safeGetIntExtra(
                intent, ShortcutHelper.EXTRA_DISPLAY_MODE, WebDisplayMode.STANDALONE);
        int orientation = IntentUtils.safeGetIntExtra(
                intent, ShortcutHelper.EXTRA_ORIENTATION, ScreenOrientationValues.DEFAULT);
        int source = sourceFromIntent(intent);
        Integer backgroundColor = colorFromLongColor(
                IntentUtils.safeGetLongExtra(intent, ShortcutHelper.EXTRA_BACKGROUND_COLOR,
                        ShortcutHelper.MANIFEST_COLOR_INVALID_OR_MISSING));
        boolean isIconGenerated = IntentUtils.safeGetBooleanExtra(
                intent, ShortcutHelper.EXTRA_IS_ICON_GENERATED, false);
        boolean isIconAdaptive = IntentUtils.safeGetBooleanExtra(
                intent, ShortcutHelper.EXTRA_IS_ICON_ADAPTIVE, false);
        boolean forceNavigation = IntentUtils.safeGetBooleanExtra(
                intent, ShortcutHelper.EXTRA_FORCE_NAVIGATION, false);

        String name = nameFromIntent(intent);
        String shortName = shortNameFromIntent(intent);

        int defaultBackgroundColor =
                SplashLayout.getDefaultBackgroundColor(ContextUtils.getApplicationContext());

        WebappExtras webappExtras = new WebappExtras(id, url, scope, new WebappIcon(icon), name,
                shortName, displayMode, orientation, source, hasValidToolbarColor, backgroundColor,
                defaultBackgroundColor, isIconGenerated, isIconAdaptive, forceNavigation);
        return new WebappIntentDataProvider(toolbarColor, webappExtras);
    }

    private WebappIntentDataProvider(int toolbarColor, WebappExtras webappExtras) {
        mToolbarColor = toolbarColor;
        mWebappExtras = webappExtras;
    }

    @Override
    public int getToolbarColor() {
        return mToolbarColor;
    }

    @Override
    public int getTitleVisibilityState() {
        return CustomTabsIntent.SHOW_PAGE_TITLE;
    }

    @Override
    @Nullable
    public WebappExtras getWebappExtras() {
        return mWebappExtras;
    }
}
