// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import android.content.Context;
import android.content.pm.PackageManager;

import org.chromium.base.Log;

import java.util.Collection;

/**
 * Helper functions for launching site-settings for websites associated with a Trusted Web Activity.
 */
public class TrustedWebActivitySettingsLauncher {
    private static final String TAG = "TwaSettingsLauncher";

    /**
     * Launches site-settings for a Trusted Web Activity app with a given package name. If the
     * app has multiple origins associated with it, the user will see a list of origins and will be
     * able to work with each of them.
     */
    public static void launchForPackageName(Context context, String packageName) {
        Integer applicationUid = getApplicationUid(context, packageName);
        if (applicationUid == null) return;

        ClientAppDataRegister register = new ClientAppDataRegister();
        Collection<String> domains = register.getDomainsForRegisteredUid(applicationUid);
        Collection<String> origins = register.getOriginsForRegisteredUid(applicationUid);
        if (domains.isEmpty() || origins.isEmpty()) {
            Log.d(TAG, "Package " + packageName + " is not associated with any origins");
            return;
        }
        launch(context, origins, domains);
    }

    /**
     * Launches site-settings for a WebApk with a given package name and associated url.
     */
    public static void launchForWebApkPackageName(
            Context context, String packageName, String webApkUrl) {
    }

    private static Integer getApplicationUid(Context context, String packageName) {
        int applicationUid;
        try {
            applicationUid = context.getPackageManager().getApplicationInfo(packageName, 0).uid;
        } catch (PackageManager.NameNotFoundException e) {
            Log.d(TAG, "Package " + packageName + " not found");
            return null;
        }
        return applicationUid;
    }

    /**
     * Same as above, but with list of associated origins and domains already retrieved.
     */
    public static void launch(Context context, Collection<String> origins,
            Collection<String> domains) {
    }

}
