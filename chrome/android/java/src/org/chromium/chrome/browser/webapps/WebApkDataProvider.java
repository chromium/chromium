// Copyright 2022 The Chromium Authors
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

import androidx.annotation.NonNull;

import org.jni_zero.CalledByNative;

import org.chromium.base.ContextUtils;
import org.chromium.base.ResettersForTesting;
import org.chromium.chrome.browser.ActivityUtils;
import org.chromium.chrome.browser.browserservices.intents.BitmapHelper;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.intents.WebappInfo;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.components.embedder_support.util.Origin;
import org.chromium.components.webapk.lib.client.WebApkValidator;
import org.chromium.components.webapps.WebApkDetailsForDefaultOfflinePage;
import org.chromium.content_public.browser.WebContents;
import org.chromium.webapk.lib.common.WebApkConstants;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/** Provides access to more detail about webapks. */
public class WebApkDataProvider {
    // Contains the details to return for an offline app when testing.
    private static WebappInfo sWebappInfoForTesting;

    // Keeps track of the data needed for the custom offline page data.
    private static class OfflineData {
        private @NonNull String mName;
        private @NonNull String mIcon;
    }

    public static void setWebappInfoForTesting(WebappInfo webappInfo) {
        sWebappInfoForTesting = webappInfo;
        ResettersForTesting.register(() -> sWebappInfoForTesting = null);
    }

    public static WebappInfo getPartialWebappInfo(String url) {
        if (sWebappInfoForTesting != null) return sWebappInfoForTesting;

        Context appContext = ContextUtils.getApplicationContext();
        String packageName = WebApkValidator.queryFirstWebApkPackage(appContext, url);
        return WebappInfo.create(
                WebApkIntentDataProviderFactory.create(
                        new Intent(),
                        packageName,
                        "",
                        WebApkConstants.ShortcutSource.UNKNOWN,
                        /* forceNavigation= */ false,
                        /* canUseSplashFromContentProvider= */ false,
                        /* shareData= */ null,
                        /* shareDataActivityClassName= */ null));
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
        result.mIcon = webAppInfo.icon().encoded();

        return result;
    }

    private static OfflineData getOfflinePageInfoForTwa(CustomTabActivity customTabActivity) {
        BrowserServicesIntentDataProvider dataProvider = customTabActivity.getIntentDataProvider();
        if (dataProvider == null) return null;

        String clientPackageName = dataProvider.getClientPackageName();
        if (clientPackageName == null) return null;

        OfflineData result = new OfflineData();
        PackageManager packageManager = ContextUtils.getApplicationContext().getPackageManager();
        try {
            result.mName =
                    packageManager
                            .getApplicationLabel(
                                    packageManager.getApplicationInfo(
                                            clientPackageName, PackageManager.GET_META_DATA))
                            .toString();
            Drawable d = packageManager.getApplicationIcon(clientPackageName);
            Bitmap bitmap =
                    Bitmap.createBitmap(
                            d.getIntrinsicWidth(), d.getIntrinsicHeight(), Bitmap.Config.ARGB_8888);
            Canvas canvas = new Canvas(bitmap);
            d.setBounds(0, 0, canvas.getWidth(), canvas.getHeight());
            d.draw(canvas);
            result.mIcon = BitmapHelper.encodeBitmapAsString(bitmap);
        } catch (PackageManager.NameNotFoundException e) {
            return null;
        }

        return result;
    }

    private static boolean isWithinScope(String url, CustomTabActivity customTabActivity) {
        BrowserServicesIntentDataProvider dataProvider = customTabActivity.getIntentDataProvider();
        Set<Origin> origins = new HashSet<>();
        origins.add(Origin.create(dataProvider.getUrlToLoad()));

        List<String> additionalOrigins = dataProvider.getTrustedWebActivityAdditionalOrigins();
        if (additionalOrigins != null) {
            for (String origin : additionalOrigins) {
                origins.add(Origin.create(origin));
            }
        }

        return origins.contains(Origin.create(url));
    }

    @CalledByNative
    public static String[] getOfflinePageInfo(int[] fields, String url, WebContents webContents) {
        Activity activity = ActivityUtils.getActivityFromWebContents(webContents);

        OfflineData offlineData = null;
        if (activity instanceof CustomTabActivity) {
            CustomTabActivity customTabActivity = (CustomTabActivity) activity;
            if (isWithinScope(url, customTabActivity)) {
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
                default:
                    fieldValues.add("No such field: " + field);
                    break;
            }
        }
        return fieldValues.toArray(new String[0]);
    }
}
