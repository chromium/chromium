// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.drawable.Drawable;

import androidx.annotation.ColorInt;
import androidx.annotation.NonNull;

import org.chromium.base.ContextUtils;
import org.chromium.base.StrictModeContext;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.browser.ActivityUtils;
import org.chromium.chrome.browser.browserservices.intents.BitmapHelper;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.intents.ColorProvider;
import org.chromium.chrome.browser.browserservices.intents.WebappInfo;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.components.webapk.lib.client.WebApkValidator;
import org.chromium.components.webapps.WebApkDetailsForDefaultOfflinePage;
import org.chromium.content_public.browser.WebContents;
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

    // Keeps track of the data needed for the custom offline page data.
    private static class OfflineData {
        private @NonNull String mName;
        private @NonNull String mIcon;
        private @NonNull long mBackgroundColor;
        private @NonNull long mThemeColor;
    }

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

    private static OfflineData getOfflinePageInfoForPwa(String url) {
        WebappInfo webAppInfo = getPartialWebappInfo(url);
        if (webAppInfo == null) return null;

        OfflineData result = new OfflineData();
        result.mName = webAppInfo.shortName();
        // Encoding the image is marked as a slow method, but this call is intentional,
        // we're encoding only a single small icon (the app icon) and the code path is
        // triggered only when the device is offline. We therefore shouldn't bother
        // jumping through hoops to make it fast.
        try (StrictModeContext ignored = StrictModeContext.allowSlowCalls()) {
            result.mIcon = webAppInfo.icon().encoded();
        }
        result.mBackgroundColor = (long) webAppInfo.backgroundColorFallbackToDefault();
        result.mThemeColor = webAppInfo.toolbarColor();

        return result;
    }

    private static OfflineData getOfflinePageInfoForTwa(CustomTabActivity customTabActivity) {
        BrowserServicesIntentDataProvider dataProvider = customTabActivity.getIntentDataProvider();
        if (dataProvider == null) return null;

        ColorProvider colorProvider = dataProvider.getColorProvider();
        String clientPackageName = dataProvider.getClientPackageName();
        if (colorProvider == null || clientPackageName == null) return null;

        OfflineData result = new OfflineData();
        result.mBackgroundColor = (long) colorProvider.getInitialBackgroundColor();
        result.mThemeColor = (long) colorProvider.getToolbarColor();

        PackageManager packageManager = ContextUtils.getApplicationContext().getPackageManager();
        try {
            result.mName = packageManager
                                   .getApplicationLabel(packageManager.getApplicationInfo(
                                           clientPackageName, PackageManager.GET_META_DATA))
                                   .toString();
            Drawable d = packageManager.getApplicationIcon(clientPackageName);
            Bitmap bitmap = Bitmap.createBitmap(
                    d.getIntrinsicWidth(), d.getIntrinsicHeight(), Bitmap.Config.ARGB_8888);
            Canvas canvas = new Canvas(bitmap);
            d.setBounds(0, 0, canvas.getWidth(), canvas.getHeight());
            d.draw(canvas);
            try (StrictModeContext ignored = StrictModeContext.allowSlowCalls()) {
                result.mIcon = BitmapHelper.encodeBitmapAsString(bitmap);
            }
        } catch (PackageManager.NameNotFoundException e) {
            return null;
        }

        return result;
    }

    @CalledByNative
    public static String[] getOfflinePageInfo(int[] fields, String url, WebContents webContents) {
        Activity activity = ActivityUtils.getActivityFromWebContents(webContents);

        OfflineData offlineData = null;
        if (activity instanceof CustomTabActivity) {
            CustomTabActivity customTabActivity = (CustomTabActivity) activity;
            if (customTabActivity.isInTwaMode()) {
                offlineData = getOfflinePageInfoForTwa(customTabActivity);
            }
        } else {
            offlineData = getOfflinePageInfoForPwa(url);
        }

        if (offlineData == null) return null;

        List<String> fieldValues = new ArrayList<String>();
        for (int field : fields) {
            switch (field) {
                case WebApkDetailsForDefaultOfflinePage.SHORT_NAME:
                    fieldValues.add(offlineData.mName);
                    break;
                case WebApkDetailsForDefaultOfflinePage.ICON:
                    fieldValues.add("data:image/png;base64," + offlineData.mIcon);
                    break;
                case WebApkDetailsForDefaultOfflinePage.BACKGROUND_COLOR:
                    fieldValues.add(colorToHexString(offlineData.mBackgroundColor));
                    break;
                case WebApkDetailsForDefaultOfflinePage.BACKGROUND_COLOR_DARK_MODE:
                    // TODO(finnur): Implement proper dark mode background colors.
                    fieldValues.add(colorToHexString(offlineData.mBackgroundColor));
                    break;
                case WebApkDetailsForDefaultOfflinePage.THEME_COLOR:
                    fieldValues.add(offlineData.mThemeColor != ColorUtils.INVALID_COLOR
                                    ? colorToHexString(offlineData.mThemeColor)
                                    : "");
                    break;
                case WebApkDetailsForDefaultOfflinePage.THEME_COLOR_DARK_MODE:
                    // TODO(finnur): Implement proper dark mode theme colors.
                    fieldValues.add(offlineData.mThemeColor != ColorUtils.INVALID_COLOR
                                    ? colorToHexString(offlineData.mThemeColor)
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
