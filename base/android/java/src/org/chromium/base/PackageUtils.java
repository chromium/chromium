// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.content.Context;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;

/**
 * This class provides package checking related methods.
 */
public class PackageUtils {
    /**
     * Retrieves the version of the given package installed on the device.
     *
     * @param context Any context.
     * @param packageName Name of the package to find.
     * @return The package's version code if found, -1 otherwise.
     */
    public static int getPackageVersion(Context context, String packageName) {
        int versionCode = -1;
        PackageManager pm = context.getPackageManager();
        try {
            PackageInfo packageInfo = pm.getPackageInfo(packageName, 0);
            if (packageInfo != null) versionCode = packageInfo.versionCode;
        } catch (PackageManager.NameNotFoundException e) {
            // Do nothing, versionCode stays -1
        }
        return versionCode;
    }

    private PackageUtils() {
        // Hide constructor
    }
}
