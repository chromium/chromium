// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.test;

import android.content.ComponentName;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.ActivityInfo;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageInfo;
import android.content.pm.ProviderInfo;
import android.content.res.Resources;
import android.net.Uri;
import android.os.Bundle;
import android.os.PatternMatcher;

import org.mockito.Mockito;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.Shadows;
import org.robolectric.shadows.ShadowPackageManager;

import org.chromium.webapk.lib.common.WebApkCommonUtils;
import org.chromium.webapk.lib.common.WebApkConstants;

/** Helper class for WebAPK JUnit tests. */
public class WebApkTestHelper {
    private static final String SHARE_TARGET_ACTIVITY_CLASS_NAME_PREFIX = "TestShareTargetActivity";
    private static final String URL_HANDLER_ACTIVITY_CLASS_NAME = "TestUrlHandlerActivity";

    /** Returns the simplest intent for launching a WebAPK. */
    public static Intent createMinimalWebApkIntent(String webApkPackageName, String url) {
        Intent intent = new Intent();
        intent.setPackage(RuntimeEnvironment.getApplication().getPackageName());
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
                Shadows.shadowOf(RuntimeEnvironment.getApplication().getPackageManager());
        Resources res = Mockito.mock(Resources.class);
        ShadowPackageManager.resources.put(packageName, res);

        // This replaces an old registration of the WebAPK, including its share target activities.
        // The share target activities are added after the package is installed because
        // ShadowPackageManager only accepts intent filters for activities added with
        // addOrUpdateActivity(), not for activities in an installed PackageInfo.
        packageManager.installPackage(
                newPackageInfo(
                        packageName,
                        metaData,
                        /* shareTargetActivityClassNames= */ null,
                        /* shareTargetMetaData= */ null));

        if (shareTargetMetaData == null) return;

        for (int i = 0; i < shareTargetMetaData.length; ++i) {
            String className = getGeneratedShareTargetActivityClassName(i);
            packageManager.addOrUpdateActivity(
                    newActivityInfo(packageName, className, shareTargetMetaData[i]));
            packageManager.addIntentFilterForActivity(
                    new ComponentName(packageName, className), newShareTargetIntentFilter());
        }
    }

    /** Returns generated share activity class name for the given index. */
    public static String getGeneratedShareTargetActivityClassName(int index) {
        return SHARE_TARGET_ACTIVITY_CLASS_NAME_PREFIX + index;
    }

    /**
     * Registers intent filter for the passed-in package name and URL. The WebAPK must have been
     * registered with {@link #registerWebApkWithMetaData} first.
     */
    public static void addIntentFilterForUrl(String packageName, String url) {
        ShadowPackageManager packageManager =
                Shadows.shadowOf(RuntimeEnvironment.getApplication().getPackageManager());
        ComponentName component = new ComponentName(packageName, URL_HANDLER_ACTIVITY_CLASS_NAME);
        packageManager.addActivityIfNotPresent(component);

        // Matches the browsable VIEW intent which WebApkValidator builds for exactly this URL.
        Uri uri = Uri.parse(url);
        IntentFilter filter = new IntentFilter(Intent.ACTION_VIEW);
        filter.addCategory(Intent.CATEGORY_BROWSABLE);
        filter.addCategory(Intent.CATEGORY_DEFAULT);
        filter.addDataScheme(uri.getScheme());
        filter.addDataAuthority(
                uri.getHost(), uri.getPort() == -1 ? null : String.valueOf(uri.getPort()));
        filter.addDataPath(uri.getPath(), PatternMatcher.PATTERN_LITERAL);
        packageManager.addIntentFilterForActivity(component, filter);
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

    /** Returns the share intent filter which a WebAPK's manifest declares for a share target. */
    private static IntentFilter newShareTargetIntentFilter() {
        IntentFilter filter = IntentFilter.create(Intent.ACTION_SEND, "text/plain");
        filter.addCategory(Intent.CATEGORY_DEFAULT);
        return filter;
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
