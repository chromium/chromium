// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.shell_apk;

import android.content.Intent;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Bundle;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowApplication;

import org.chromium.components.webapk.lib.common.WebApkMetaDataKeys;
import org.chromium.webapk.lib.common.WebApkConstants;
import org.chromium.webapk.shell_apk.h2o.H2OMainActivity;
import org.chromium.webapk.test.WebApkTestHelper;

/**
 * Unit tests for {@link MainActivity}.
 *
 * <p>Note: In real word, |loggedIntentUrlParam| is set to be nonempty iff intent url is outside of
 * the scope specified in the Android manifest, so in the test we always have these two conditions
 * together.
 */
@RunWith(RobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public final class MainActivityTest {
    private static final String BROWSER_PACKAGE_NAME = "com.android.chrome";

    private PackageManager mPackageManager;
    private TestBrowserInstaller mBrowserInstaller = new TestBrowserInstaller();

    @Before
    public void setUp() {
        mPackageManager = RuntimeEnvironment.application.getPackageManager();
        mBrowserInstaller.installModernBrowser(BROWSER_PACKAGE_NAME);
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
        WebApkTestHelper.registerWebApkWithMetaData(
                WebApkUtilsTest.WEBAPK_PACKAGE_NAME, bundle, /* shareTargetMetaData= */ null);

        Intent launchIntent = new Intent(Intent.ACTION_VIEW, Uri.parse(intentStartUrl));
        Robolectric.buildActivity(H2OMainActivity.class, launchIntent).create();

        Intent startedActivityIntent = ShadowApplication.getInstance().getNextStartedActivity();
        assertWebApkLaunched(startedActivityIntent, expectedStartUrl);
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
        WebApkTestHelper.registerWebApkWithMetaData(
                WebApkUtilsTest.WEBAPK_PACKAGE_NAME, bundle, /* shareTargetMetaData= */ null);

        Intent launchIntent = new Intent(Intent.ACTION_VIEW, Uri.parse(intentStartUrl));
        Robolectric.buildActivity(H2OMainActivity.class, launchIntent).create();

        Intent startedActivityIntent = ShadowApplication.getInstance().getNextStartedActivity();
        assertWebApkLaunched(startedActivityIntent, expectedStartUrl);
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
        WebApkTestHelper.registerWebApkWithMetaData(
                WebApkUtilsTest.WEBAPK_PACKAGE_NAME, bundle, /* shareTargetMetaData= */ null);

        Intent launchIntent = new Intent(Intent.ACTION_VIEW, Uri.parse(intentStartUrl));
        Robolectric.buildActivity(H2OMainActivity.class, launchIntent).create();

        Intent startedActivityIntent = ShadowApplication.getInstance().getNextStartedActivity();
        assertWebApkLaunched(startedActivityIntent, expectedStartUrl);
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
        WebApkTestHelper.registerWebApkWithMetaData(
                WebApkUtilsTest.WEBAPK_PACKAGE_NAME, bundle, /* shareTargetMetaData= */ null);

        Intent launchIntent = new Intent(Intent.ACTION_VIEW, Uri.parse(intentStartUrl));
        Robolectric.buildActivity(H2OMainActivity.class, launchIntent).create();

        Intent startedActivityIntent = ShadowApplication.getInstance().getNextStartedActivity();
        assertWebApkLaunched(startedActivityIntent, intentStartUrl);
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
        WebApkTestHelper.registerWebApkWithMetaData(
                WebApkUtilsTest.WEBAPK_PACKAGE_NAME, bundle, /* shareTargetMetaData= */ null);

        Intent launchIntent = new Intent(Intent.ACTION_VIEW, Uri.parse(intentStartUrl));
        Robolectric.buildActivity(H2OMainActivity.class, launchIntent).create();

        Intent startedActivityIntent = ShadowApplication.getInstance().getNextStartedActivity();
        assertWebApkLaunched(startedActivityIntent, expectedStartUrl);
    }

    /**
     * Tests that if the only installed browser does not support WebAPKs that the browser is
     * launched in tabbed mode.
     */
    @Test
    public void testShouldLaunchInTabNonChromeBrowser() throws Exception {
        final String nonChromeBrowserPackageName = "com.crazy.browser";
        mBrowserInstaller.setInstalledBrowserWithVersion(
                nonChromeBrowserPackageName, "10000.0.000.0");

        final String startUrl = "https://www.google.com/";

        Bundle bundle = new Bundle();
        bundle.putString(WebApkMetaDataKeys.START_URL, startUrl);
        bundle.putString(WebApkMetaDataKeys.SCOPE, startUrl);
        // Unbound WebAPK, no runtime host.
        WebApkTestHelper.registerWebApkWithMetaData(
                WebApkUtilsTest.WEBAPK_PACKAGE_NAME, bundle, /* shareTargetMetaData= */ null);

        Intent launchIntent = new Intent(Intent.ACTION_VIEW, Uri.parse(startUrl));
        Robolectric.buildActivity(H2OMainActivity.class, launchIntent).create();

        Intent startedActivityIntent = ShadowApplication.getInstance().getNextStartedActivity();
        assertTabbedBrowserLaunched(startedActivityIntent, nonChromeBrowserPackageName, startUrl);
    }

    /**
     * Tests that a WebAPK should not be launched as a tab if Chrome's version is higher or equal to
     * {@link WebApkUtils#MINIMUM_REQUIRED_CHROME_VERSION}.
     */
    @Test
    public void testShouldNotLaunchInTabWithNewVersionOfChrome() throws Exception {
        mBrowserInstaller.setInstalledBrowserWithVersion(BROWSER_PACKAGE_NAME, "57.0.000.0");

        final String startUrl = "https://www.google.com/";

        Bundle bundle = new Bundle();
        bundle.putString(WebApkMetaDataKeys.START_URL, startUrl);
        bundle.putString(WebApkMetaDataKeys.SCOPE, startUrl);
        bundle.putString(WebApkMetaDataKeys.RUNTIME_HOST, BROWSER_PACKAGE_NAME);
        WebApkTestHelper.registerWebApkWithMetaData(
                WebApkUtilsTest.WEBAPK_PACKAGE_NAME, bundle, /* shareTargetMetaData= */ null);

        Intent launchIntent = new Intent(Intent.ACTION_VIEW, Uri.parse(startUrl));
        Robolectric.buildActivity(H2OMainActivity.class, launchIntent).create();

        Intent startedActivityIntent = ShadowApplication.getInstance().getNextStartedActivity();
        assertWebApkLaunched(startedActivityIntent, startUrl);
    }

    /**
     * Check that extras which should be propagated from the WebAPK launch intent to the browser
     * launch intent are in fact propagated.
     */
    @Test
    public void testPropagatedDeepLinkExtras() {
        final String startUrl = "https://www.google.com/";

        Bundle extrasToPropagate = new Bundle();
        // WebAPK should not override these extras if they are provided in the WebAPK launch intent.
        extrasToPropagate.putBoolean(WebApkConstants.EXTRA_FORCE_NAVIGATION, true);
        extrasToPropagate.putLong(WebApkConstants.EXTRA_WEBAPK_LAUNCH_TIME, 314159);

        // WebAPK should copy to the browser launch intent arbirtary extras provided in the WebAPK
        // launch intent.
        extrasToPropagate.putString("randomKey", "randomValue");

        Bundle bundle = new Bundle();
        bundle.putString(WebApkMetaDataKeys.START_URL, startUrl);
        bundle.putString(WebApkMetaDataKeys.SCOPE, startUrl);
        bundle.putString(WebApkMetaDataKeys.RUNTIME_HOST, BROWSER_PACKAGE_NAME);
        WebApkTestHelper.registerWebApkWithMetaData(
                WebApkUtilsTest.WEBAPK_PACKAGE_NAME, bundle, /* shareTargetMetaData= */ null);

        Intent launchIntent = new Intent(Intent.ACTION_VIEW, Uri.parse(startUrl));
        launchIntent.putExtras((Bundle) extrasToPropagate.clone());
        Robolectric.buildActivity(H2OMainActivity.class, launchIntent).create();

        Intent startedActivityIntent = ShadowApplication.getInstance().getNextStartedActivity();
        assertWebApkLaunched(startedActivityIntent, startUrl);
        Bundle actualExtras = startedActivityIntent.getExtras();
        Assert.assertNotNull(actualExtras);
        for (String key : extrasToPropagate.keySet()) {
            Assert.assertEquals(extrasToPropagate.get(key), actualExtras.get(key));
        }
    }

    /**
     * Asserts that the passed-in intent is an intent to launch the passed-in browser package in
     * tabbed mode.
     */
    private void assertTabbedBrowserLaunched(
            Intent intent, String browserPackageName, String expectedStartUrl) {
        Assert.assertEquals(browserPackageName, intent.getPackage());
        Assert.assertEquals(Intent.ACTION_VIEW, intent.getAction());
        Assert.assertEquals(expectedStartUrl, intent.getDataString());
    }

    /**
     * Asserts that the passed in intent is an intent to launch {@link BROWSER_PACKAGE_NAME} in
     * WebAPK mode.
     */
    private void assertWebApkLaunched(Intent intent, String expectedStartUrl) {
        Assert.assertEquals(BROWSER_PACKAGE_NAME, intent.getPackage());
        Assert.assertEquals(HostBrowserLauncher.ACTION_START_WEBAPK, intent.getAction());
        Assert.assertEquals(expectedStartUrl, intent.getStringExtra(WebApkConstants.EXTRA_URL));
    }
}
