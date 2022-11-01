// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.customtabsclient;

import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.net.Uri;
import android.text.TextUtils;
import android.util.Pair;

import java.util.ArrayList;
import java.util.List;

/**
 * Helper class for fetching supporting package for Custom Tabs along with
 * saving the last used package.
 */
public class CustomTabsPackageHelper {
    private static final String CUSTOM_TABS_SERVICE_TAG =
            "android.support.customtabs.action.CustomTabsService";
    private static final String PREF_KEY_LAST_USED_PACKAGE = "Package";

    private final PackageManager mPackageManager;
    private final SharedPreferences mSharedPreferences;

    public CustomTabsPackageHelper(Context context, SharedPreferences sharedPreferences) {
        mPackageManager = context.getPackageManager();
        mSharedPreferences = sharedPreferences;
    }

    public List<Pair<String, String>> getCustomTabsSupportingPackages() {
        Intent webPageIntent = new Intent(Intent.ACTION_VIEW, Uri.parse("http://www.example.com"));
        List<ResolveInfo> resolvedActivityList =
                mPackageManager.queryIntentActivities(webPageIntent, PackageManager.MATCH_ALL);

        // Adds queried packages to list, puts last used package at 0 position if applicable
        String lastUsedPackage = mSharedPreferences.getString(PREF_KEY_LAST_USED_PACKAGE, "");
        List<Pair<String, String>> supportingPackages = new ArrayList<>();
        for (ResolveInfo info : resolvedActivityList) {
            final Intent serviceIntent = new Intent();
            serviceIntent.setAction(CUSTOM_TABS_SERVICE_TAG);
            serviceIntent.setPackage(info.activityInfo.packageName);

            if (mPackageManager.resolveService(serviceIntent, 0) != null) {
                String label = info.loadLabel(mPackageManager).toString();
                String packageName = info.activityInfo.packageName;
                boolean isLastUsedPackage = TextUtils.equals(label, lastUsedPackage);
                Pair<String, String> appPair = Pair.create(label, packageName);
                if (isLastUsedPackage) {
                    supportingPackages.add(0, appPair);
                } else {
                    supportingPackages.add(appPair);
                }
            }
        }
        return supportingPackages;
    }

    public void saveLastUsedPackage(final String lastUsedPackage) {
        SharedPreferences.Editor editor = mSharedPreferences.edit();
        editor.putString(PREF_KEY_LAST_USED_PACKAGE, lastUsedPackage);
        editor.apply();
    }
}
