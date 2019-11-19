// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Build;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.LargeTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.DeferredStartupHandler;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.tab.TabTestUtils;
import org.chromium.chrome.browser.tab.TabWebContentsDelegateAndroid;
import org.chromium.chrome.browser.util.IntentUtils;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.browser.webapps.WebApkInfoBuilder;
import org.chromium.content_public.browser.test.NativeLibraryTestRule;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.webapk.lib.common.WebApkConstants;

/** Tests for WebApkActivity. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public final class WebApkActivityTest {
    private static final String TEST_WEBAPK_PACKAGE_NAME = "org.chromium.webapk.for.testing";
    private static final String TEST_WEBAPK_ID =
            WebApkConstants.WEBAPK_ID_PREFIX + TEST_WEBAPK_PACKAGE_NAME;

    public final WebApkActivityTestRule mActivityTestRule = new WebApkActivityTestRule();
    public final NativeLibraryTestRule mNativeLibraryTestRule = new NativeLibraryTestRule();

    @Before
    public void setUp() {
        WebApkUpdateManager.setUpdatesEnabledForTesting(false);
        mActivityTestRule.getEmbeddedTestServerRule().setServerUsesHttps(true);

        // WebAPK is not installed. Ensure that WebappRegistry#unregisterOldWebapps() does not
        // delete the WebAPK's shared preferences.
        SharedPreferences sharedPrefs = ContextUtils.getApplicationContext().getSharedPreferences(
                WebappRegistry.REGISTRY_FILE_NAME, Context.MODE_PRIVATE);
        sharedPrefs.edit()
                .putLong(WebappRegistry.KEY_LAST_CLEANUP, System.currentTimeMillis())
                .apply();
    }

    /**
     * Test that navigating a WebAPK to a URL which is outside of the WebAPK's scope shows the
     * toolbar.
     */
    @Test
    @LargeTest
    @Feature({"WebApk"})
    public void testLaunchAndNavigateOutsideScope() throws Exception {
        WebApkActivity webApkActivity = mActivityTestRule.startWebApkActivity(createWebApkInfo(
                getTestServerUrl("scope_a/page_1.html"), getTestServerUrl("scope_a/")));
        WebappActivityTestRule.assertToolbarShowState(webApkActivity, false);

        // We navigate outside scope and expect CCT toolbar to show on top of WebApkActivity.
        String outOfScopeUrl = getTestServerUrl("manifest_test_page.html");
        mActivityTestRule.runJavaScriptCodeInCurrentTab(
                "window.top.location = '" + outOfScopeUrl + "'");

        ChromeTabUtils.waitForTabPageLoaded(webApkActivity.getActivityTab(), outOfScopeUrl);
        WebappActivityTestRule.assertToolbarShowState(webApkActivity, true);
    }

    /**
     * Test launching a WebAPK. Test that opening a url within scope through window.open() will open
     * a CCT.
     */
    @Test
    @LargeTest
    @Feature({"WebApk"})
    public void testLaunchAndOpenNewWindowInScope() throws Exception {
        String scopeUrl = getTestServerUrl("scope_a/");
        String inScopeUrl = getTestServerUrl("scope_a/page_1.html");
        WebApkActivity webApkActivity =
                mActivityTestRule.startWebApkActivity(createWebApkInfo(inScopeUrl, scopeUrl));

        WebappActivityTestRule.jsWindowOpen(mActivityTestRule.getActivity(), inScopeUrl);

        CustomTabActivity customTabActivity =
                ChromeActivityTestRule.waitFor(CustomTabActivity.class);
        ChromeTabUtils.waitForTabPageLoaded(customTabActivity.getActivityTab(), inScopeUrl);
        Assert.assertTrue(
                "Sending to external handlers needs to be enabled for redirect back (e.g. OAuth).",
                IntentUtils.safeGetBooleanExtra(customTabActivity.getIntent(),
                        CustomTabIntentDataProvider.EXTRA_SEND_TO_EXTERNAL_DEFAULT_HANDLER, false));
    }

    /**
     * Test launching a WebAPK. Test that opening a url off scope through window.open() will open a
     * CCT, and in scope urls will stay in the CCT.
     */
    @Test
    @LargeTest
    @Feature({"WebApk"})
    public void testLaunchAndNavigationInNewWindowOffandInScope() throws Exception {
        String scopeUrl = getTestServerUrl("scope_a/");
        String inScopeUrl = getTestServerUrl("scope_a/page_1.html");
        String offScopeUrl = getTestServerUrl("scope_b/scope_b.html");
        WebApkActivity webApkActivity =
                mActivityTestRule.startWebApkActivity(createWebApkInfo(inScopeUrl, scopeUrl));

        WebappActivityTestRule.jsWindowOpen(mActivityTestRule.getActivity(), offScopeUrl);
        CustomTabActivity customTabActivity =
                ChromeActivityTestRule.waitFor(CustomTabActivity.class);
        ChromeTabUtils.waitForTabPageLoaded(customTabActivity.getActivityTab(), offScopeUrl);

        JavaScriptUtils.executeJavaScriptAndWaitForResult(
                customTabActivity.getActivityTab().getWebContents(),
                String.format("window.location.href='%s'", inScopeUrl));
        ChromeTabUtils.waitForTabPageLoaded(customTabActivity.getActivityTab(), inScopeUrl);
    }

    /**
     * Test that on first launch:
     * - the "WebApk.LaunchInterval" histogram is not recorded (because there is no previous launch
     *   to compute the interval from).
     * - the "last used" time is updated (to compute future "launch intervals").
     */
    @Test
    @LargeTest
    @Feature({"WebApk"})
    public void testLaunchIntervalHistogramNotRecordedOnFirstLaunch() {
        android.util.Log.e("ABCD", "Start");
        final String histogramName = "WebApk.LaunchInterval";
        WebApkActivity webApkActivity = mActivityTestRule.startWebApkActivity(createWebApkInfo(
                getTestServerUrl("manifest_test_page.html"), getTestServerUrl("/")));

        CriteriaHelper.pollUiThread(new Criteria("Deferred startup never completed") {
            @Override
            public boolean isSatisfied() {
                return DeferredStartupHandler.getInstance().isDeferredStartupCompleteForApp()
                        && WebappRegistry.getInstance().getWebappDataStorage(TEST_WEBAPK_ID)
                        != null;
            }
        });
        Assert.assertEquals(0, RecordHistogram.getHistogramTotalCountForTesting(histogramName));
        WebappDataStorage storage =
                WebappRegistry.getInstance().getWebappDataStorage(TEST_WEBAPK_ID);
        Assert.assertNotEquals(WebappDataStorage.TIMESTAMP_INVALID, storage.getLastUsedTimeMs());
        android.util.Log.e("ABCD", "Start2");
    }

    /** Test that the "WebApk.LaunchInterval" histogram is recorded on susbequent launches. */
    @Test
    @LargeTest
    @Feature({"WebApk"})
    public void testLaunchIntervalHistogramRecordedOnSecondLaunch() throws Exception {
        mNativeLibraryTestRule.loadNativeLibraryNoBrowserProcess();

        final String histogramName = "WebApk.LaunchInterval2";
        final String packageName = "org.chromium.webapk.test";

        WebappDataStorage storage = registerWithStorage(TEST_WEBAPK_ID);
        storage.setHasBeenLaunched();
        storage.updateLastUsedTime();
        Assert.assertEquals(0, RecordHistogram.getHistogramTotalCountForTesting(histogramName));

        WebApkActivity webApkActivity = mActivityTestRule.startWebApkActivity(createWebApkInfo(
                getTestServerUrl("manifest_test_page.html"), getTestServerUrl("/")));

        CriteriaHelper.pollUiThread(new Criteria("Deferred startup never completed") {
            @Override
            public boolean isSatisfied() {
                return DeferredStartupHandler.getInstance().isDeferredStartupCompleteForApp();
            }
        });

        Assert.assertEquals(1, RecordHistogram.getHistogramTotalCountForTesting(histogramName));
    }

    /**
     * Test the L+ logic in {@link TabWebContentsDelegateAndroid#activateContents} for bringing
     * WebAPK to the foreground.
     */
    @LargeTest
    @Test
    public void testActivateWebApkLPlus() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP) return;

        // Launch WebAPK.
        WebApkActivity webApkActivity = mActivityTestRule.startWebApkActivity(createWebApkInfo(
                getTestServerUrl("manifest_test_page.html"), getTestServerUrl("/")));

        Class<? extends ChromeActivity> mainClass = ChromeTabbedActivity.class;

        // Move WebAPK to the background by launching Chrome.
        Intent intent = new Intent(InstrumentationRegistry.getTargetContext(), mainClass);
        intent.setFlags(
                Intent.FLAG_ACTIVITY_NEW_TASK | ApiCompatibilityUtils.getActivityNewDocumentFlag());
        InstrumentationRegistry.getTargetContext().startActivity(intent);
        ChromeActivityTestRule.waitFor(mainClass);

        TabWebContentsDelegateAndroid tabDelegate =
                TabTestUtils.getTabWebContentsDelegate(webApkActivity.getActivityTab());
        tabDelegate.activateContents();

        // WebApkActivity should have been brought back to the foreground.
        ChromeActivityTestRule.waitFor(WebApkActivity.class);
    }

    private WebApkInfo createWebApkInfo(String startUrl, String scopeUrl) {
        WebApkInfoBuilder webApkInfoBuilder =
                new WebApkInfoBuilder(TEST_WEBAPK_PACKAGE_NAME, startUrl);
        webApkInfoBuilder.setScope(scopeUrl);
        return webApkInfoBuilder.build();
    }

    private String getTestServerUrl(String relativeUrl) {
        return mActivityTestRule.getEmbeddedTestServerRule().getServer().getURL(
                "/chrome/test/data/banners/" + relativeUrl);
    }

    /** Register WebAPK with WebappDataStorage */
    private WebappDataStorage registerWithStorage(final String webappId) throws Exception {
        TestFetchStorageCallback callback = new TestFetchStorageCallback();
        WebappRegistry.getInstance().register(webappId, callback);
        callback.waitForCallback(0);
        return WebappRegistry.getInstance().getWebappDataStorage(webappId);
    }
}
