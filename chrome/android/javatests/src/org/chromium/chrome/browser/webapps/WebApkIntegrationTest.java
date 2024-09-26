// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import static org.junit.Assert.assertEquals;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;
import android.net.Uri;
import android.os.Build;
import android.os.IBinder;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.LargeTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;

import org.chromium.base.CommandLine;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.test.MockCertVerifierRuleAndroid;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.components.webapk.lib.client.WebApkValidator;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.webapk.lib.client.WebApkServiceConnectionManager;
import org.chromium.webapk.lib.runtime_library.IWebApkApi;

import java.util.concurrent.TimeoutException;

/** Integration tests for WebAPK feature. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@DoNotBatch(reason = "Tests activity start behavior")
public class WebApkIntegrationTest {
    public final WebApkActivityTestRule mActivityTestRule = new WebApkActivityTestRule();

    public MockCertVerifierRuleAndroid mCertVerifierRule =
            new MockCertVerifierRuleAndroid(0 /* net::OK */);

    @Rule
    public RuleChain mRuleChain =
            RuleChain.emptyRuleChain().around(mActivityTestRule).around(mCertVerifierRule);

    private static final long STARTUP_TIMEOUT = 15000L;

    @Before
    public void setUp() {
        mActivityTestRule.getEmbeddedTestServerRule().setServerUsesHttps(true);
        Uri mapToUri =
                Uri.parse(mActivityTestRule.getEmbeddedTestServerRule().getServer().getURL("/"));
        CommandLine.getInstance()
                .appendSwitchWithValue(
                        ContentSwitches.HOST_RESOLVER_RULES, "MAP * " + mapToUri.getAuthority());
        WebApkValidator.setDisableValidationForTesting(true);
    }

    /** Tests that sending deep link intent to WebAPK launches WebAPK Activity. */
    @Test
    @LargeTest
    @Feature({"Webapps"})
    public void testDeepLink() {
        String pageUrl = "https://pwa-directory.appspot.com/defaultresponse";

        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(pageUrl));
        intent.setPackage("org.chromium.webapk.test");
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        ApplicationProvider.getApplicationContext().startActivity(intent);

        WebappActivity lastActivity =
                ChromeActivityTestRule.waitFor(WebappActivity.class, STARTUP_TIMEOUT);
        Assert.assertEquals(ActivityType.WEB_APK, lastActivity.getActivityType());
        Assert.assertEquals(pageUrl, lastActivity.getIntentDataProvider().getUrlToLoad());
    }

    /**
     * Tests that Chrome will trampoline out to WebAPKs if they exist but are not verified. See
     * https://crbug.com/1232514
     */
    @Test
    @LargeTest
    @Feature({"Webapps"})
    @MinAndroidSdkLevel(Build.VERSION_CODES.S)
    public void testWebApkTrampoline() {
        Context targetContext = ApplicationProvider.getApplicationContext();
        String pageUrl = "https://pwa-directory.appspot.com/defaultresponse";

        // Make a standard browsable Intent to a page within the WebAPK's scope.
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(pageUrl));
        intent.addCategory(Intent.CATEGORY_BROWSABLE);

        // FLAG_ACTIVITY_NEW_TASK required because we're launching from a non-Activity context.
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        // We need to set the component name to make sure the Intent ends up in the Chrome build
        // that we're testing. We can't set the package name, because our launch code has special
        // handling if the package name is set and is equal to Chrome
        // (see RedirectHandler#updateIntent).
        intent.setComponent(new ComponentName(targetContext, ChromeLauncherActivity.class));

        targetContext.startActivity(intent);

        // Check we end up in the WebAPK.
        ChromeActivityTestRule.waitFor(WebappActivity.class, STARTUP_TIMEOUT);
    }

    /** Tests launching WebAPK via POST share intent. */
    @Test
    @LargeTest
    @Feature({"Webapps"})
    @DisabledTest(message = "https://crbug.com/1112352")
    public void testShare() throws TimeoutException {
        final String sharedSubject = "Fun tea parties";
        final String sharedText = "Boston";
        final String expectedShareUrl = "https://pwa-directory.appspot.com/echoall";

        Intent intent = new Intent(Intent.ACTION_SEND);
        intent.setPackage("org.chromium.webapk.test");
        intent.setType("text/plain");
        intent.putExtra(Intent.EXTRA_SUBJECT, sharedSubject);
        intent.putExtra(Intent.EXTRA_TEXT, sharedText);
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        ApplicationProvider.getApplicationContext().startActivity(intent);

        WebappActivity lastActivity =
                ChromeActivityTestRule.waitFor(WebappActivity.class, STARTUP_TIMEOUT);
        Assert.assertEquals(ActivityType.WEB_APK, lastActivity.getActivityType());

        Tab tab = lastActivity.getActivityTab();
        ChromeTabUtils.waitForTabPageLoaded(tab, expectedShareUrl);
        String postDataJson =
                JavaScriptUtils.executeJavaScriptAndWaitForResult(
                        tab.getWebContents(), "document.getElementsByTagName('pre')[0].innerText");
        assertEquals("\"title=Fun+tea+parties\\ntext=Boston\\n\"", postDataJson);
    }

    /**
     * Integration test for the WebAPK service loading logic. The WebAPK service loads its
     * implementation from a dex stored in Chrome's APK.
     */
    @Test
    @LargeTest
    @Feature({"Webapps"})
    @DisableIf.Device(DeviceFormFactor.TABLET) // crbug.com/362218524
    public void testWebApkServiceIntegration() throws Exception {
        Context context = ApplicationProvider.getApplicationContext();

        // Launch WebAPK in order to cache host browser.
        Intent intent =
                new Intent(
                        Intent.ACTION_VIEW,
                        Uri.parse("https://pwa-directory.appspot.com/defaultresponse"));
        intent.setPackage("org.chromium.webapk.test");
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        context.startActivity(intent);
        ChromeActivityTestRule.waitFor(WebappActivity.class);

        // Extract small icon id from WebAPK resources.
        Resources res =
                context.getPackageManager().getResourcesForApplication("org.chromium.webapk.test");
        final int expectedSmallIconId =
                res.getIdentifier("notification_badge", "drawable", "org.chromium.webapk.test");

        CallbackHelper callbackHelper = new CallbackHelper();
        WebApkServiceConnectionManager connectionManager =
                new WebApkServiceConnectionManager(
                        TaskTraits.UI_DEFAULT,
                        WebApkServiceClient.CATEGORY_WEBAPK_API,
                        /* action= */ null);
        connectionManager.connect(
                ApplicationProvider.getApplicationContext(),
                "org.chromium.webapk.test",
                new WebApkServiceConnectionManager.ConnectionCallback() {
                    @Override
                    public void onConnected(IBinder api) {
                        try {
                            int actualSmallIconId =
                                    IWebApkApi.Stub.asInterface(api).getSmallIconId();
                            assertEquals(actualSmallIconId, expectedSmallIconId);
                            callbackHelper.notifyCalled();
                        } catch (Exception e) {
                            throw new AssertionError(
                                    "WebApkService binder call threw exception", e);
                        }
                    }
                });

        callbackHelper.waitForNext();
    }
}
