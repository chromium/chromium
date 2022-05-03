// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.content.Context;
import android.content.Intent;

import androidx.annotation.ColorInt;

import org.chromium.base.ContextUtils;
import org.chromium.base.StrictModeContext;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.browser.browserservices.intents.WebappInfo;
import org.chromium.components.webapk.lib.client.WebApkValidator;
import org.chromium.components.webapps.WebApkDetailsForDefaultOfflinePage;
import org.chromium.ui.util.ColorUtils;
import org.chromium.webapk.lib.common.WebApkConstants;

import java.util.ArrayList;
import java.util.List;

/**
 * Provides access to more detail about webapks.
 */
public class WebApkDataProvider {
    // Contains the details to return for an offline app when testing.
    private static WebappInfo sWebappInfoForTesting;

    public static void setWebappInfoForTesting(WebappInfo webappInfo) {
        sWebappInfoForTesting = webappInfo;
    }

    /**
     * Converts a color value to a hex string.
     * @param color The color to convert.
     * @return The RGB values of the color, as string presented in hex (prefixed with #).
     *         For example: '#FF0000' (for red).
     */
    private static String colorToHexString(@ColorInt long color) {
        return String.format("#%02X%02X%02X", (((color) >> 16) & 0xFF), (((color) >> 8) & 0xFF),
                (((color) >> 0) & 0xFF));
    }

    private static WebappInfo getPartialWebappInfo(String url) {
        if (sWebappInfoForTesting != null) return sWebappInfoForTesting;

        Context appContext = ContextUtils.getApplicationContext();
        String packageName = WebApkValidator.queryFirstWebApkPackage(appContext, url);
        return WebappInfo.create(WebApkIntentDataProviderFactory.create(new Intent(), packageName,
                "", WebApkConstants.ShortcutSource.UNKNOWN, false /* forceNavigation */,
                false /* isSplashProvidedByWebApk */, null /* shareData */,
                null /* shareDataActivityClassName */));
    }

    @CalledByNative
    public static String[] getOfflinePageInfo(int[] fields, String url) {
        WebappInfo webAppInfo = getPartialWebappInfo(url);
        if (webAppInfo == null) {
            return null;
        }

        String shortName = webAppInfo.shortName();
        int backgroundColor = webAppInfo.backgroundColorFallbackToDefault();
        Long toolbarColor = webAppInfo.toolbarColor();

        List<String> fieldValues = new ArrayList<String>();
        for (int field : fields) {
            switch (field) {
                case WebApkDetailsForDefaultOfflinePage.SHORT_NAME:
                    fieldValues.add(shortName);
                    break;
                case WebApkDetailsForDefaultOfflinePage.ICON:
                    // Encoding the image is marked as a slow method, but this call is intentional,
                    // we're encoding only a single small icon (the app icon) and the code path is
                    // triggered only when the device is offline. We therefore shouldn't bother
                    // jumping through hoops to make it fast.
                    try (StrictModeContext ignored = StrictModeContext.allowSlowCalls()) {
                        fieldValues.add("data:image/png;base64," + webAppInfo.icon().encoded());
                    }
                    break;
                case WebApkDetailsForDefaultOfflinePage.BACKGROUND_COLOR:
                    fieldValues.add(colorToHexString(backgroundColor));
                    break;
                case WebApkDetailsForDefaultOfflinePage.BACKGROUND_COLOR_DARK_MODE:
                    // TODO(finnur): Implement proper dark mode background colors.
                    fieldValues.add(colorToHexString(backgroundColor));
                    break;
                case WebApkDetailsForDefaultOfflinePage.THEME_COLOR:
                    fieldValues.add(toolbarColor != ColorUtils.INVALID_COLOR
                                    ? colorToHexString(toolbarColor)
                                    : "");
                    break;
                case WebApkDetailsForDefaultOfflinePage.THEME_COLOR_DARK_MODE:
                    // TODO(finnur): Implement proper dark mode theme colors.
                    fieldValues.add(toolbarColor != ColorUtils.INVALID_COLOR
                                    ? colorToHexString(toolbarColor)
                                    : "");
                    break;
                default:
                    fieldValues.add("No such field: " + field);
                    break;
            }
        }
        return fieldValues.toArray(new String[0]);
    }
}
