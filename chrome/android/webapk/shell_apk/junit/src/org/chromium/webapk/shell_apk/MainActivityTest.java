// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.shell_apk;

import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageInfo;
import android.content.pm.ResolveInfo;
import android.net.Uri;
import android.os.Bundle;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowApplication;
import org.robolectric.shadows.ShadowPackageManager;

import org.chromium.testing.local.LocalRobolectricTestRunner;
import org.chromium.webapk.lib.common.WebApkConstants;
import org.chromium.webapk.lib.common.WebApkMetaDataKeys;
import org.chromium.webapk.test.WebApkTestHelper;

/** Unit tests for {@link MainActivity}.
 *
 * Note: In real word, |loggedIntentUrlParam| is set to be nonempty iff intent url is outside of the
 * scope specified in the Android manifest, so in the test we always have these two conditions
 * together.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE, packageName = WebApkUtilsTest.WEBAPK_PACKAGE_NAME)
public final class MainActivityTest {
    private ShadowPackageManager mPackageManager;
    private static final String BROWSER_PACKAGE_NAME = "com.android.chrome";

    @Before
    public void setUp() {
        mPackageManager = Shadows.shadowOf(RuntimeEnvironment.application.getPackageManager());
        installBrowser(BROWSER_PACKAGE_NAME);
    }

    /**
     * Test that MainActivity uses the manifest start URL and appends the intent url as a paramater,
     * if intent URL scheme does not match the scope url.
     */
    @Test
    public void testIntentUrlOutOfScopeBecauseOfScheme() {
        final String intentStartUrl = "geo:0,0?q=Kenora";
        final String manifestStartUrl = "https://www.google.com/index.html";
        final String manifestScope = "https://www.google.com/";
        final String expectedStartUrl =
                "https://www.google.com/index.html?originalUrl=geo%3A0%2C0%3Fq%3DKenora";

        Bundle bundle = new Bundle();
        bundle.putString(WebApkMetaDataKeys.START_URL, manifestStartUrl);
        bundle.putString(WebApkMetaDataKeys.SCOPE, manifestScope);
        bundle.putString(WebApkMetaDataKeys.RUNTIME_HOST, BROWSER_PACKAGE_NAME);
        bundle.putString(WebApkMetaDataKeys.LOGGED_INTENT_URL_PARAM, "originalUrl");
        WebApkTestHelper.registerWebApkWithMetaData(WebApkUtilsTest.WEBAPK_PACKAGE_NAME, bundle);

        Intent launchIntent = new Intent(Intent.ACTION_VIEW, Uri.parse(intentStartUrl));
        Robolectric.buildActivity(MainActivity.class, launchIntent).create();

        assertWebApkLaunched(expectedStartUrl);
    }

    /**
     * Test that MainActivity uses the manifest start URL and appends the intent url as a paramater,
     * if intent URL path is outside of the scope specified in the Android Manifest.
     */
    @Test
    public void testIntentUrlOutOfScopeBecauseOfPath() {
        final String intentStartUrl = "https://www.google.com/maps/";
        final String manifestStartUrl = "https://www.google.com/maps/contrib/startUrl";
        final String manifestScope = "https://www.google.com/maps/contrib/";
        final String expectedStartUrl =
                "https://www.google.com/maps/contrib/startUrl?originalUrl=https%3A%2F%2Fwww.google.com%2Fmaps%2F";

        Bundle bundle = new Bundle();
        bundle.putString(WebApkMetaDataKeys.START_URL, manifestStartUrl);
        bundle.putString(WebApkMetaDataKeys.SCOPE, manifestScope);
        bundle.putString(WebApkMetaDataKeys.RUNTIME_HOST, BROWSER_PACKAGE_NAME);
        bundle.putString(WebApkMetaDataKeys.LOGGED_INTENT_URL_PARAM, "originalUrl");
        WebApkTestHelper.registerWebApkWithMetaData(WebApkUtilsTest.WEBAPK_PACKAGE_NAME, bundle);

        Intent launchIntent = new Intent(Intent.ACTION_VIEW, Uri.parse(intentStartUrl));
        Robolectric.buildActivity(MainActivity.class, launchIntent).create();

        assertWebApkLaunched(expectedStartUrl);
    }

    /**
     * Tests that the intent URL is rewritten if |LoggedIntentUrlParam| is set, even though the
     * intent URL is inside the scope specified in the Android Manifest.
     */
    @Test
    public void testRewriteStartUrlInsideScope() {
        final String intentStartUrl = "https://www.google.com/maps/address?A=a";
        final String manifestStartUrl = "https://www.google.com/maps/startUrl";
        final String manifestScope = "https://www.google.com/maps";
        final String expectedStartUrl =
                "https://www.google.com/maps/startUrl?originalUrl=https%3A%2F%2Fwww.google.com%2Fmaps%2Faddress%3FA%3Da";

        Bundle bundle = new Bundle();
        bundle.putString(WebApkMetaDataKeys.START_URL, manifestStartUrl);
        bundle.putString(WebApkMetaDataKeys.SCOPE, manifestScope);
        bundle.putString(WebApkMetaDataKeys.RUNTIME_HOST, BROWSER_PACKAGE_NAME);
        bundle.putString(WebApkMetaDataKeys.LOGGED_INTENT_URL_PARAM, "originalUrl");
        WebApkTestHelper.registerWebApkWithMetaData(WebApkUtilsTest.WEBAPK_PACKAGE_NAME, bundle);

        Intent launchIntent = new Intent(Intent.ACTION_VIEW, Uri.parse(intentStartUrl));
        Robolectric.buildActivity(MainActivity.class, launchIntent).create();

        assertWebApkLaunched(expectedStartUrl);
    }

    /**
     * Tests that the intent URL is not rewritten again if the query parameter to append is part of
     * the intent URL when |LoggedIntentUrlParam| is set.
     */
    @Test
    public void testNotRewriteStartUrlWhenContainsTheQueryParameterToAppend() {
        final String intentStartUrl =
                "https://www.google.com/maps/startUrl?originalUrl=https%3A%2F%2Fwww.google.com%2Fmaps%2Faddress%3FA%3Da";
        final String manifestStartUrl = "https://www.google.com/maps/startUrl";
        final String manifestScope = "https://www.google.com/maps";

        Bundle bundle = new Bundle();
        bundle.putString(WebApkMetaDataKeys.START_URL, manifestStartUrl);
        bundle.putString(WebApkMetaDataKeys.SCOPE, manifestScope);
        bundle.putString(WebApkMetaDataKeys.RUNTIME_HOST, BROWSER_PACKAGE_NAME);
        bundle.putString(WebApkMetaDataKeys.LOGGED_INTENT_URL_PARAM, "originalUrl");
        WebApkTestHelper.registerWebApkWithMetaData(WebApkUtilsTest.WEBAPK_PACKAGE_NAME, bundle);

        Intent launchIntent = new Intent(Intent.ACTION_VIEW, Uri.parse(intentStartUrl));
        Robolectric.buildActivity(MainActivity.class, launchIntent).create();

        assertWebApkLaunched(intentStartUrl);
    }

    /**
     * Test that MainActivity uses the manifest start URL and appends the intent url as a paramater,
     * if intent URL host includes unicode characters, and the host name is different from the scope
     * url host specified in the Android Manifest. In particular, MainActivity should not escape
     * unicode characters.
     */
    @Test
    public void testRewriteUnicodeHost() {
        final String intentStartUrl = "https://www.google.com/";
        final String manifestStartUrl = "https://www.☺.com/";
        final String scope = "https://www.☺.com/";
        final String expectedStartUrl =
                "https://www.☺.com/?originalUrl=https%3A%2F%2Fwww.google.com%2F";

        Bundle bundle = new Bundle();
        bundle.putString(WebApkMetaDataKeys.START_URL, manifestStartUrl);
        bundle.putString(WebApkMetaDataKeys.SCOPE, scope);
        bundle.putString(WebApkMetaDataKeys.RUNTIME_HOST, BROWSER_PACKAGE_NAME);
        bundle.putString(WebApkMetaDataKeys.LOGGED_INTENT_URL_PARAM, "originalUrl");
        WebApkTestHelper.registerWebApkWithMetaData(WebApkUtilsTest.WEBAPK_PACKAGE_NAME, bundle);

        Intent launchIntent = new Intent(Intent.ACTION_VIEW, Uri.parse(intentStartUrl));
        Robolectric.buildActivity(MainActivity.class, launchIntent).create();

        assertWebApkLaunched(expectedStartUrl);
    }

    /**
     * Tests that a WebAPK should be launched as a tab if Chrome's version number is lower than
     * {@link HostBrowserUtils#MINIMUM_REQUIRED_CHROME_VERSION}.
     */
    @Test
    public void testShouldLaunchInTabWhenChromeVersionIsTooLow() throws Exception {
        final String startUrl = "https://www.google.com/";
        final String oldVersionName = "56.0.000.0";

        Bundle bundle = new Bundle();
        bundle.putString(WebApkMetaDataKeys.START_URL, startUrl);
        bundle.putString(WebApkMetaDataKeys.SCOPE, startUrl);
        bundle.putString(WebApkMetaDataKeys.RUNTIME_HOST, BROWSER_PACKAGE_NAME);
        WebApkTestHelper.registerWebApkWithMetaData(WebApkUtilsTest.WEBAPK_PACKAGE_NAME, bundle);

        mPackageManager.getPackageInfo(BROWSER_PACKAGE_NAME, 0).versionName = oldVersionName;

        Intent launchIntent = new Intent(Intent.ACTION_VIEW, Uri.parse(startUrl));
        Robolectric.buildActivity(MainActivity.class, launchIntent).create();

        Intent startActivityIntent = ShadowApplication.getInstance().getNextStartedActivity();
        Assert.assertEquals(BROWSER_PACKAGE_NAME, startActivityIntent.getPackage());
        Assert.assertEquals(Intent.ACTION_VIEW, startActivityIntent.getAction());
        Assert.assertEquals(startUrl, startActivityIntent.getDataString());
    }

    /**
     * Tests that a WebAPK should not be launched as a tab if Chrome's version is higher or equal to
     * {@link WebApkUtils#MINIMUM_REQUIRED_CHROME_VERSION}.
     */
    @Test
    public void testShouldNotLaunchInTabWithNewVersionOfChrome() throws Exception {
        final String startUrl = "https://www.google.com/";
        final String newVersionName = "57.0.000.0";

        Bundle bundle = new Bundle();
        bundle.putString(WebApkMetaDataKeys.START_URL, startUrl);
        bundle.putString(WebApkMetaDataKeys.SCOPE, startUrl);
        bundle.putString(WebApkMetaDataKeys.RUNTIME_HOST, BROWSER_PACKAGE_NAME);
        WebApkTestHelper.registerWebApkWithMetaData(WebApkUtilsTest.WEBAPK_PACKAGE_NAME, bundle);

        mPackageManager.getPackageInfo(BROWSER_PACKAGE_NAME, 0).versionName = newVersionName;

        Intent launchIntent = new Intent(Intent.ACTION_VIEW, Uri.parse(startUrl));
        Robolectric.buildActivity(MainActivity.class, launchIntent).create();

        assertWebApkLaunched(startUrl);
    }

    /** Asserts that {@link BROWSER_PACKAGE_NAME} was launched in WebAPK mode. */
    private void assertWebApkLaunched(String expectedStartUrl) {
        Intent startActivityIntent = ShadowApplication.getInstance().getNextStartedActivity();
        Assert.assertEquals(BROWSER_PACKAGE_NAME, startActivityIntent.getPackage());
        Assert.assertEquals(
                HostBrowserLauncher.ACTION_START_WEBAPK, startActivityIntent.getAction());
        Assert.assertEquals(
                expectedStartUrl, startActivityIntent.getStringExtra(WebApkConstants.EXTRA_URL));
    }

    private void installBrowser(String browserPackageName) {
        Intent intent = WebApkUtils.getQueryInstalledBrowsersIntent();
        Shadows.shadowOf(RuntimeEnvironment.application.getPackageManager())
                .addResolveInfoForIntent(intent, newResolveInfo(browserPackageName));
        Shadows.shadowOf(RuntimeEnvironment.application.getPackageManager())
                .addPackage(newPackageInfo(browserPackageName));
    }

    private static ResolveInfo newResolveInfo(String packageName) {
        ActivityInfo activityInfo = new ActivityInfo();
        activityInfo.packageName = packageName;
        ResolveInfo resolveInfo = new ResolveInfo();
        resolveInfo.activityInfo = activityInfo;
        return resolveInfo;
    }

    private static PackageInfo newPackageInfo(String packageName) {
        PackageInfo packageInfo = new PackageInfo();
        packageInfo.packageName = packageName;
        packageInfo.versionName = "10000.0.0.0";
        packageInfo.applicationInfo = new ApplicationInfo();
        packageInfo.applicationInfo.enabled = true;
        return packageInfo;
    }
}
