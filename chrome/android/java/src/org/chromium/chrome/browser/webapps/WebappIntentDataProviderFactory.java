// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.content.Intent;
import android.text.TextUtils;

import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.Log;
import org.chromium.blink.mojom.DisplayMode;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.intents.WebappConstants;
import org.chromium.chrome.browser.browserservices.intents.WebappExtras;
import org.chromium.chrome.browser.browserservices.intents.WebappIcon;
import org.chromium.chrome.browser.browserservices.intents.WebappIntentUtils;
import org.chromium.components.webapps.ShortcutSource;
import org.chromium.device.mojom.ScreenOrientationLockType;
import org.chromium.ui.util.ColorUtils;
import org.chromium.webapk.lib.common.splash.SplashLayout;

/** Factory for building {@link BrowserServicesIntentDataProvider} for homescreen shortcuts. */
public class WebappIntentDataProviderFactory {
    private static final String TAG = "WebappInfo";

    private static int sourceFromIntent(Intent intent) {
        int source =
                IntentUtils.safeGetIntExtra(
                        intent, WebappConstants.EXTRA_SOURCE, ShortcutSource.UNKNOWN);
        if (source >= ShortcutSource.COUNT) {
            source = ShortcutSource.UNKNOWN;
        }
        return source;
    }

    private static String titleFromIntent(Intent intent) {
        // The reference to title has been kept for reasons of backward compatibility. For intents
        // and shortcuts which were created before we utilized the concept of name and shortName,
        // we set the name and shortName to be the title.
        String title = IntentUtils.safeGetStringExtra(intent, WebappConstants.EXTRA_TITLE);
        return title == null ? "" : title;
    }

    private static String nameFromIntent(Intent intent) {
        String name = IntentUtils.safeGetStringExtra(intent, WebappConstants.EXTRA_NAME);
        return name == null ? titleFromIntent(intent) : name;
    }

    private static String shortNameFromIntent(Intent intent) {
        String shortName = IntentUtils.safeGetStringExtra(intent, WebappConstants.EXTRA_SHORT_NAME);
        return shortName == null ? titleFromIntent(intent) : shortName;
    }

    /**
     * Construct a BrowserServicesIntentDataProvider.
     * @param intent Intent containing info about the app.
     */
    public static BrowserServicesIntentDataProvider create(Intent intent) {
        String id = WebappIntentUtils.getIdForHomescreenShortcut(intent);
        String url = IntentUtils.safeGetStringExtra(intent, WebappConstants.EXTRA_URL);
        if (id == null || url == null) {
            Log.e(TAG, "Incomplete data provided: " + id + ", " + url);
            return null;
        }

        long themeColor =
                IntentUtils.safeGetLongExtra(
                        intent, WebappConstants.EXTRA_THEME_COLOR, ColorUtils.INVALID_COLOR);
        boolean hasValidToolbarColor = WebappIntentUtils.isLongColorValid(themeColor);
        int toolbarColor =
                hasValidToolbarColor
                        ? (int) themeColor
                        : WebappIntentDataProvider.getDefaultToolbarColor();
        long darkThemeColor =
                IntentUtils.safeGetLongExtra(
                        intent, WebappConstants.EXTRA_DARK_THEME_COLOR, ColorUtils.INVALID_COLOR);
        boolean hasValidDarkToolbarColor = WebappIntentUtils.isLongColorValid(darkThemeColor);
        int darkToolbarColor =
                hasValidDarkToolbarColor
                        ? (int) darkThemeColor
                        : WebappIntentDataProvider.getDefaultDarkToolbarColor();

        String icon = IntentUtils.safeGetStringExtra(intent, WebappConstants.EXTRA_ICON);

        String scope = IntentUtils.safeGetStringExtra(intent, WebappConstants.EXTRA_SCOPE);
        if (TextUtils.isEmpty(scope)) {
            scope = ShortcutHelper.getScopeFromUrl(url);
        }

        @DisplayMode.EnumType
        int displayMode =
                IntentUtils.safeGetIntExtra(
                        intent, WebappConstants.EXTRA_DISPLAY_MODE, DisplayMode.STANDALONE);
        int orientation =
                IntentUtils.safeGetIntExtra(
                        intent,
                        WebappConstants.EXTRA_ORIENTATION,
                        ScreenOrientationLockType.DEFAULT);
        int source = sourceFromIntent(intent);
        Integer backgroundColor =
                WebappIntentUtils.colorFromLongColor(
                        IntentUtils.safeGetLongExtra(
                                intent,
                                WebappConstants.EXTRA_BACKGROUND_COLOR,
                                ColorUtils.INVALID_COLOR));
        Integer darkBackgroundColor =
                WebappIntentUtils.colorFromLongColor(
                        IntentUtils.safeGetLongExtra(
                                intent,
                                WebappConstants.EXTRA_DARK_BACKGROUND_COLOR,
                                ColorUtils.INVALID_COLOR));
        boolean isIconGenerated =
                IntentUtils.safeGetBooleanExtra(
                        intent, WebappConstants.EXTRA_IS_ICON_GENERATED, false);
        boolean isIconAdaptive =
                IntentUtils.safeGetBooleanExtra(
                        intent, WebappConstants.EXTRA_IS_ICON_ADAPTIVE, false);
        boolean forceNavigation =
                IntentUtils.safeGetBooleanExtra(
                        intent, WebappConstants.EXTRA_FORCE_NAVIGATION, false);

        String name = nameFromIntent(intent);
        String shortName = shortNameFromIntent(intent);

        int defaultBackgroundColor =
                SplashLayout.getDefaultBackgroundColor(ContextUtils.getApplicationContext());

        WebappExtras webappExtras =
                new WebappExtras(
                        id,
                        url,
                        scope,
                        new WebappIcon(icon, /* isTrusted= */ true),
                        name,
                        shortName,
                        displayMode,
                        orientation,
                        source,
                        backgroundColor,
                        darkBackgroundColor,
                        defaultBackgroundColor,
                        isIconGenerated,
                        isIconAdaptive,
                        forceNavigation);
        return new WebappIntentDataProvider(
                intent,
                toolbarColor,
                hasValidToolbarColor,
                darkToolbarColor,
                hasValidDarkToolbarColor,
                /* shareData= */ null,
                webappExtras,
                /* webApkExtras= */ null);
    }
}
