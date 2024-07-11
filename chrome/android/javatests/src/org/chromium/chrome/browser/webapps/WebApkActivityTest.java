// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import static org.junit.Assert.assertEquals;

import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.LargeTest;
import androidx.test.runner.lifecycle.Stage;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.cc.input.BrowserControlsState;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.TabTestUtils;
import org.chromium.chrome.browser.tab.TabWebContentsDelegateAndroid;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.browser.webapps.WebApkIntentDataProviderBuilder;
import org.chromium.components.permissions.PermissionDialogController;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.webapk.lib.common.WebApkConstants;

import java.util.concurrent.TimeoutException;

/** Tests for WebAPK {@link WebappActivity}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public final class WebApkActivityTest {
    private static final String TEST_WEBAPK_PACKAGE_NAME = "org.chromium.webapk.for.testing";
    private static final String TEST_WEBAPK_ID =
            WebApkConstants.WEBAPK_ID_PREFIX + TEST_WEBAPK_PACKAGE_NAME;

    @Rule public final WebApkActivityTestRule mActivityTestRule = new WebApkActivityTestRule();

    @Before
    public void setUp() {
        mActivityTestRule.getEmbeddedTestServerRule().setServerUsesHttps(true);

        // WebAPK is not installed. Ensure that WebappRegistry#unregisterOldWebapps() does not
        // delete the WebAPK's shared preferences.
        SharedPreferences sharedPrefs =
                ContextUtils.getApplicationContext()
                        .getSharedPreferences(
                                WebappRegistry.REGISTRY_FILE_NAME, Context.MODE_PRIVATE);
        sharedPrefs
                .edit()
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
        WebappActivity webApkActivity =
                mActivityTestRule.startWebApkActivity(
                        createIntentDataProvider(
                                getTestServerUrl("scope_a/page_1.html"),
                                getTestServerUrl("scope_a/")));
        assertEquals(
                BrowserControlsState.HIDDEN,
                WebappActivityTestRule.getToolbarShowState(webApkActivity));

        // We navigate outside scope and expect CCT toolbar to show on top of WebAPK Activity.
        String outOfScopeUrl = getTestServerUrl("manifest_test_page.html");
        mActivityTestRule.runJavaScriptCodeInCurrentTab(
                "window.top.location = '" + outOfScopeUrl + "'");

        ChromeTabUtils.waitForTabPageLoaded(webApkActivity.getActivityTab(), outOfScopeUrl);
        WebappActivityTestRule.assertToolbarShownMaybeHideable(webApkActivity);
    }

    /**
     * Test the L+ logic in {@link TabWebContentsDelegateAndroid#activateContents} for bringing
     * WebAPK to the foreground.
     */
    @LargeTest
    @Test
    public void testActivateWebApkLPlus() throws Exception {
        // Launch WebAPK.
        WebappActivity webApkActivity =
                mActivityTestRule.startWebApkActivity(
                        createIntentDataProvider(
                                getTestServerUrl("manifest_test_page.html"),
                                getTestServerUrl("/")));

        Class<? extends ChromeActivity> mainClass = ChromeTabbedActivity.class;

        // Move WebAPK to the background by launching Chrome.
        Intent intent = new Intent(ApplicationProvider.getApplicationContext(), mainClass);
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_NEW_DOCUMENT);
        ApplicationProvider.getApplicationContext().startActivity(intent);
        ChromeActivityTestRule.waitFor(mainClass);

        ApplicationTestUtils.waitForActivityState(webApkActivity, Stage.STOPPED);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabWebContentsDelegateAndroid tabDelegate =
                            TabTestUtils.getTabWebContentsDelegate(webApkActivity.getActivityTab());
                    tabDelegate.activateContents();
                });

        // WebAPK Activity should have been brought back to the foreground.
        ChromeActivityTestRule.waitFor(WebappActivity.class);
    }

    /** Test a permission dialog can be correctly presented. */
    @Test
    @LargeTest
    @DisabledTest(message = "The test is flaky, see b/324471058.")
    public void testShowPermissionPrompt() throws TimeoutException {
        EmbeddedTestServer server = mActivityTestRule.getEmbeddedTestServerRule().getServer();
        String url = server.getURL("/content/test/data/android/permission_navigation.html");
        String baseUrl = server.getURL("/content/test/data/android/");
        WebappActivity activity =
                mActivityTestRule.startWebApkActivity(createIntentDataProvider(url, baseUrl));
        mActivityTestRule.runJavaScriptCodeInCurrentTab("requestGeolocationPermission()");
        CriteriaHelper.pollUiThread(
                () -> PermissionDialogController.getInstance().isDialogShownForTest(),
                "Permission prompt did not appear in allotted time");
        Assert.assertEquals(
                "Only App modal dialog is supported on web apk",
                activity.getModalDialogManager()
                        .getPresenterForTest(ModalDialogManager.ModalDialogType.APP),
                activity.getModalDialogManager().getCurrentPresenterForTest());
    }

    private BrowserServicesIntentDataProvider createIntentDataProvider(
            String startUrl, String scopeUrl) {
        WebApkIntentDataProviderBuilder intentDataProviderBuilder =
                new WebApkIntentDataProviderBuilder(TEST_WEBAPK_PACKAGE_NAME, startUrl);
        intentDataProviderBuilder.setScope(scopeUrl);
        return intentDataProviderBuilder.build();
    }

    private String getTestServerUrl(String relativeUrl) {
        return mActivityTestRule
                .getEmbeddedTestServerRule()
                .getServer()
                .getURL("/chrome/test/data/banners/" + relativeUrl);
    }

    /** Register WebAPK with WebappDataStorage */
    private WebappDataStorage registerWithStorage(final String webappId) throws Exception {
        TestFetchStorageCallback callback = new TestFetchStorageCallback();
        WebappRegistry.getInstance().register(webappId, callback);
        callback.waitForCallback(0);
        return WebappRegistry.getInstance().getWebappDataStorage(webappId);
    }
}
