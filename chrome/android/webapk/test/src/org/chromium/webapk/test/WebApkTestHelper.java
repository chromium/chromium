// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.test;

import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageInfo;
import android.content.pm.ProviderInfo;
import android.content.pm.ResolveInfo;
import android.content.res.Resources;
import android.os.Bundle;

import org.mockito.Mockito;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.Shadows;
import org.robolectric.shadows.ShadowPackageManager;

import org.chromium.webapk.lib.common.WebApkCommonUtils;
import org.chromium.webapk.lib.common.WebApkConstants;

import java.net.URISyntaxException;

/** Helper class for WebAPK JUnit tests. */
public class WebApkTestHelper {
    private static final String SHARE_TARGET_ACTIVITY_CLASS_NAME_PREFIX = "TestShareTargetActivity";

    /** Returns the simplest intent for launching a WebAPK. */
    public static Intent createMinimalWebApkIntent(String webApkPackageName, String url) {
        Intent intent = new Intent();
        intent.setPackage(RuntimeEnvironment.application.getPackageName());
        intent.putExtra(WebApkConstants.EXTRA_WEBAPK_PACKAGE_NAME, webApkPackageName);
        intent.putExtra(WebApkConstants.EXTRA_URL, url);
        return intent;
    }

    /**
     * Registers WebAPK. This function also creates an empty resource for the WebAPK.
     *
     * @param packageName The package to register
     * @param metaData Bundle with application-level meta data from WebAPK's Android Manifest.
     * @param shareTargetMetaData Bundles with meta data for the share target activities. Null if
     *     the WebAPK does not have any share target activities.
     */
    public static void registerWebApkWithMetaData(
            String packageName, Bundle metaData, Bundle[] shareTargetMetaData) {
        ShadowPackageManager packageManager =
                Shadows.shadowOf(RuntimeEnvironment.application.getPackageManager());
        Resources res = Mockito.mock(Resources.class);
        ShadowPackageManager.resources.put(packageName, res);

        Intent shareIntent = new Intent();
        shareIntent.setAction(Intent.ACTION_SEND);
        shareIntent.setPackage(packageName);

        // Remove old data in case that we are overwriting an old WebAPK registration.
        packageManager.removeResolveInfosForIntent(shareIntent, packageName);

        String[] shareTargetActivityClassNames = null;
        if (shareTargetMetaData != null) {
            shareTargetActivityClassNames = new String[shareTargetMetaData.length];
            for (int i = 0; i < shareTargetMetaData.length; ++i) {
                shareTargetActivityClassNames[i] = getGeneratedShareTargetActivityClassName(i);
                packageManager.addResolveInfoForIntent(
                        shareIntent,
                        newResolveInfo(
                                packageName,
                                shareTargetActivityClassNames[i],
                                shareTargetMetaData[i]));
            }
        }

        packageManager.addPackage(
                newPackageInfo(
                        packageName, metaData, shareTargetActivityClassNames, shareTargetMetaData));
    }

    /** Returns generated share activity class name for the given index. */
    public static String getGeneratedShareTargetActivityClassName(int index) {
        return SHARE_TARGET_ACTIVITY_CLASS_NAME_PREFIX + index;
    }

    /** Registers intent filter for the passed-in package name and URL. */
    public static void addIntentFilterForUrl(String packageName, String url) {
        ShadowPackageManager packageManager =
                Shadows.shadowOf(RuntimeEnvironment.application.getPackageManager());
        try {
            Intent deepLinkIntent = Intent.parseUri(url, Intent.URI_INTENT_SCHEME);
            deepLinkIntent.addCategory(Intent.CATEGORY_BROWSABLE);
            deepLinkIntent.setPackage(packageName);
            packageManager.addResolveInfoForIntent(
                    deepLinkIntent, newResolveInfo(packageName, null, null));
        } catch (URISyntaxException e) {
        }
    }

    /** Sets the resource for the given package name. */
    public static void setResource(String packageName, Resources res) {
        ShadowPackageManager.resources.put(packageName, res);
    }

    public static PackageInfo newPackageInfo(
            String webApkPackageName,
            Bundle metaData,
            String[] shareTargetActivityClassNames,
            Bundle[] shareTargetMetaData) {
        ApplicationInfo applicationInfo = new ApplicationInfo();
        applicationInfo.metaData = metaData;
        PackageInfo packageInfo = new PackageInfo();
        packageInfo.packageName = webApkPackageName;
        packageInfo.applicationInfo = applicationInfo;

        if (shareTargetMetaData != null) {
            packageInfo.activities = new ActivityInfo[shareTargetMetaData.length];
            for (int i = 0; i < shareTargetMetaData.length; ++i) {
                packageInfo.activities[i] =
                        newActivityInfo(
                                webApkPackageName,
                                shareTargetActivityClassNames[i],
                                shareTargetMetaData[i]);
            }
        }

        packageInfo.providers =
                new ProviderInfo[] {newSplashContentProviderInfo(webApkPackageName)};

        return packageInfo;
    }

    private static ResolveInfo newResolveInfo(
            String packageName, String activityClassName, Bundle activityMetaData) {
        ResolveInfo resolveInfo = new ResolveInfo();
        resolveInfo.activityInfo =
                newActivityInfo(packageName, activityClassName, activityMetaData);
        return resolveInfo;
    }

    private static ActivityInfo newActivityInfo(
            String packageName, String activityClassName, Bundle metaData) {
        ActivityInfo activityInfo = new ActivityInfo();
        activityInfo.packageName = packageName;
        activityInfo.name = activityClassName;
        activityInfo.metaData = metaData;
        return activityInfo;
    }

    private static ProviderInfo newSplashContentProviderInfo(String webApkPackageName) {
        ProviderInfo providerInfo = new ProviderInfo();
        providerInfo.authority =
                WebApkCommonUtils.generateSplashContentProviderAuthority(webApkPackageName);
        providerInfo.packageName = webApkPackageName;
        return providerInfo;
    }
}
