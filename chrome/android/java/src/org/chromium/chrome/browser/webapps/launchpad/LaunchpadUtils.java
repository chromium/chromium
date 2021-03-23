// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps.launchpad;

import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;

import org.chromium.base.Log;
import org.chromium.chrome.browser.webapps.WebApkIntentDataProviderFactory;
import org.chromium.chrome.browser.webapps.WebappInfo;
import org.chromium.components.webapk.lib.client.WebApkValidator;
import org.chromium.components.webapps.ShortcutSource;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 *  Utility class for launchpad.
 */
public class LaunchpadUtils {
    private static final String TAG = "LaunchpadUtils";
    private static final String CATEGORY_WEBAPK_API = "android.intent.category.WEBAPK_API";

    private LaunchpadUtils() {}

    /**
     * Returns a list of |LaunchpadItem| with information for each installed WebAPK.
     * This retrieves all installed WebAPKs by querying apps matches the WebAPK intent filter,
     * and check whether it passes the signature checks.
     */
    public static List<LaunchpadItem> retrieveWebApks(Context context) {
        List<LaunchpadItem> apps = new ArrayList<LaunchpadItem>();
        PackageManager packageManager = context.getPackageManager();

        Intent intent = new Intent(Intent.ACTION_MAIN);
        intent.addCategory(CATEGORY_WEBAPK_API);
        for (ResolveInfo info :
                packageManager.queryIntentServices(intent, PackageManager.MATCH_ALL)) {
            if (info.serviceInfo.packageName != null
                    && WebApkValidator.isValidV1WebApk(context, info.serviceInfo.packageName)) {
                try {
                    PackageInfo packageInfo =
                            packageManager.getPackageInfo(info.serviceInfo.packageName, 0);
                    WebappInfo webApkInfo =
                            WebappInfo.create(WebApkIntentDataProviderFactory.create(new Intent(),
                                    packageInfo.packageName, "", ShortcutSource.UNKNOWN,
                                    false /* forceNavigation */,
                                    false /* isSplashProvidedByWebApk */, null /* shareData */,
                                    null /* shareDataActivityClassName */));
                    if (webApkInfo != null) {
                        LaunchpadItem item = new LaunchpadItem(webApkInfo.webApkPackageName(),
                                webApkInfo.shortName(), webApkInfo.name(), webApkInfo.url(),
                                webApkInfo.icon().bitmap());
                        apps.add(item);
                    }
                } catch (PackageManager.NameNotFoundException e) {
                    Log.e(TAG, info.serviceInfo.packageName + " doesn't exist");
                }
            }
        }

        // Sort the list to make apps in alphabetical order.
        Collections.sort(apps, (a, b) -> a.shortName.compareTo(b.shortName));
        return apps;
    }
}
