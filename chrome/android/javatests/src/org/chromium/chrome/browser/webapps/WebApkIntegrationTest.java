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
import android.provider.Browser;
import android.text.TextUtils;
import android.util.Pair;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.LargeTest;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.CommandLine;
import org.chromium.base.IntentUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.banners.AppMenuVerbiage;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.test.MockCertVerifierRuleAndroid;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.components.webapk.lib.client.WebApkValidator;
import org.chromium.components.webapk.proto.WebApkProto;
import org.chromium.components.webapps.AddToHomescreenCoordinator;
import org.chromium.components.webapps.AddToHomescreenDialogView;
import org.chromium.components.webapps.AddToHomescreenProperties;
import org.chromium.components.webapps.AddToHomescreenViewDelegate;
import org.chromium.components.webapps.AppType;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.net.test.util.TestWebServer;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.webapk.lib.client.WebApkNavigationClient;
import org.chromium.webapk.lib.client.WebApkServiceConnectionManager;
import org.chromium.webapk.lib.runtime_library.IWebApkApi;

import java.util.ArrayList;
import java.util.List;
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
        // JavatestsWebApk.apk (//chrome/android/webapk/shell_apk:javatests_webapk, package
        // org.chromium.webapk.test) has scope_url = "https://pwa-directory.appspot.com/" and
        // <intent-filter> host pwa-directory.appspot.com statically compiled into its
        // AndroidManifest.xml at GN build time (from
        // chrome/android/webapk/shell_apk/manifest/javatest_manifest_config_delta.json).
        // HOST_RESOLVER_RULES maps pwa-directory.appspot.com to the local EmbeddedTestServer
        // (127.0.0.1:<port>) so Android PackageManager intent-filter matching and WebApkValidator
        // match the WebAPK's compiled-in scope while Chrome loads the test page from the local
        // EmbeddedTestServer.
        CommandLine.getInstance()
                .appendSwitchWithValue(
                        ContentSwitches.HOST_RESOLVER_RULES,
                        "MAP * "
                                + mapToUri.getAuthority()
                                + ", EXCLUDE 127.0.0.1, EXCLUDE localhost");
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
     * https://crbug.com/40191153
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
    @DisabledTest(message = "https://crbug.com/40709668")
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
    @DisableIf.Device(DeviceFormFactor.TABLET_OR_DESKTOP) // crbug.com/362218524
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
                            assertEquals(expectedSmallIconId, actualSmallIconId);
                            callbackHelper.notifyCalled();
                        } catch (Exception e) {
                            throw new AssertionError(
                                    "WebApkService binder call threw exception", e);
                        }
                    }
                });

        callbackHelper.waitForNext();
    }

    /**
     * Test {@link AddToHomescreenCoordinator} subclass that automatically submits the install
     * dialog once manifest and icon data have been fetched and validated.
     */
    private static class TestAddToHomescreenCoordinator extends AddToHomescreenCoordinator {
        private String mTitle;
        private final @AppType int mExpectedDialogType;

        TestAddToHomescreenCoordinator(
                WebContents webContents,
                Context context,
                WindowAndroid windowAndroid,
                ModalDialogManager modalDialogManager,
                String title,
                @AppType int expectedDialogType) {
            super(webContents, context, windowAndroid, modalDialogManager);
            mTitle = title;
            mExpectedDialogType = expectedDialogType;
        }

        @Override
        protected AddToHomescreenDialogView initView(AddToHomescreenViewDelegate delegate) {
            return new AddToHomescreenDialogView(
                    getContextForTests(), getModalDialogManagerForTests(), delegate) {
                private boolean mSubmitted;

                @Override
                protected void setTitle(String title) {
                    super.setTitle(title);
                    if (TextUtils.isEmpty(mTitle)) {
                        mTitle = title;
                    }
                }

                @Override
                protected void setCanSubmit(boolean canSubmit) {
                    super.setCanSubmit(canSubmit);
                    if (!canSubmit || mSubmitted) {
                        return;
                    }
                    mSubmitted = true;
                    if (getPropertyModelForTesting() != null) {
                        Assert.assertEquals(
                                mExpectedDialogType,
                                (int)
                                        getPropertyModelForTesting()
                                                .get(AddToHomescreenProperties.TYPE));
                    }
                    PostTask.postTask(
                            TaskTraits.UI_DEFAULT, () -> mDelegate.onAddToHomescreen(mTitle));
                }
            };
        }
    }

    /**
     * End-to-end integration test verifying that a WebAPK can be installed from a browser tab and
     * launched into a standalone {@link WebappActivity}, correctly matching {@code
     * window.matchMedia('(display-mode: standalone)').matches === true} without leaving the
     * originating browser window with zero tabs.
     */
    @Test
    @LargeTest
    @Feature({"Webapps"})
    public void testInstallAndLaunchWebApk() throws Exception {
        TestWebServer webServer = TestWebServer.start();
        try {
            byte[] responseProto =
                    WebApkProto.WebApkResponse.newBuilder()
                            .setPackageName("org.chromium.webapk.test")
                            .setVersion("1")
                            .setToken("token")
                            .build()
                            .toByteArray();
            List<Pair<String, String>> responseHeaders = new ArrayList<>();
            responseHeaders.add(Pair.create("Content-Type", "application/x-protobuf"));
            String serverUrl =
                    webServer.setResponse("/webapkserver", responseProto, responseHeaders);
            CommandLine.getInstance().appendSwitchWithValue("webapk-server-url", serverUrl);

            // JavatestsWebApk.apk (//chrome/android/webapk/shell_apk:javatests_webapk, package
            // org.chromium.webapk.test) has scope_url = "https://pwa-directory.appspot.com/" and
            // <intent-filter> host pwa-directory.appspot.com statically compiled into its
            // AndroidManifest.xml at GN build time (from
            // chrome/android/webapk/shell_apk/manifest/javatest_manifest_config_delta.json).
            // HOST_RESOLVER_RULES in setUp() maps pwa-directory.appspot.com to the local
            // EmbeddedTestServer (127.0.0.1:<port>) so Android PackageManager intent-filter
            // matching and WebApkValidator match the WebAPK's compiled-in scope while Chrome
            // loads the test page from the local EmbeddedTestServer.
            String pageUrl =
                    "https://pwa-directory.appspot.com/chrome/test/data/banners/manifest_test_page.html";

            ChromeTabbedActivity.interceptMoveTaskToBackForTesting();

            Context context = ApplicationProvider.getApplicationContext();
            Intent browserIntent = new Intent(Intent.ACTION_VIEW, Uri.parse("about:blank"));
            browserIntent.setComponent(new ComponentName(context, ChromeTabbedActivity.class));
            browserIntent.putExtra(Browser.EXTRA_APPLICATION_ID, context.getPackageName());
            browserIntent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            IntentUtils.addTrustedIntentExtras(browserIntent);
            context.startActivity(browserIntent);

            ChromeTabbedActivity tabbedActivity =
                    ChromeActivityTestRule.waitFor(ChromeTabbedActivity.class, STARTUP_TIMEOUT);
            ChromeActivityTestRule.waitForActivityCompletelyLoaded(tabbedActivity);
            Tab tab = ThreadUtils.runOnUiThreadBlocking(() -> tabbedActivity.getActivityTab());
            ChromeTabUtils.loadUrlOnUiThread(tab, pageUrl);
            ChromeTabUtils.waitForTabPageLoaded(tab, pageUrl);

            HistogramWatcher installWatcher =
                    HistogramWatcher.newSingleRecordWatcher("WebApk.Install.InstallEvent", 3);

            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        boolean started =
                                new TestAddToHomescreenCoordinator(
                                                tab.getWebContents(),
                                                tabbedActivity,
                                                tabbedActivity.getWindowAndroid(),
                                                tabbedActivity.getModalDialogManager(),
                                                "",
                                                AppType.WEBAPK)
                                        .showForAppMenu(AppMenuVerbiage.APP_MENU_OPTION_INSTALL);
                        Assert.assertTrue("AddToHomescreenCoordinator failed to start", started);
                    });

            installWatcher.pollInstrumentationThreadUntilSatisfied();

            // If WebApkInstallService did not already auto-launch the installed WebAPK into a
            // WebappActivity, simulate the user launching the installed app.
            if (!(ApplicationStatus.getLastTrackedFocusedActivity() instanceof WebappActivity)) {
                Intent launchIntent =
                        WebApkNavigationClient.createLaunchWebApkIntent(
                                "org.chromium.webapk.test", pageUrl, false);
                context.startActivity(launchIntent);
            }

            WebappActivity webappActivity =
                    ChromeActivityTestRule.waitFor(WebappActivity.class, STARTUP_TIMEOUT);
            Assert.assertEquals(ActivityType.WEB_APK, webappActivity.getActivityType());

            CriteriaHelper.pollUiThread(
                    () -> {
                        Tab candidateTab = webappActivity.getActivityTab();
                        Criteria.checkThat(candidateTab, Matchers.notNullValue());
                        Criteria.checkThat(candidateTab.getWebContents(), Matchers.notNullValue());
                    },
                    STARTUP_TIMEOUT,
                    CriteriaHelper.DEFAULT_POLLING_INTERVAL);
            Tab webappTab =
                    ThreadUtils.runOnUiThreadBlocking(() -> webappActivity.getActivityTab());
            ChromeTabUtils.waitForTabPageLoaded(webappTab, pageUrl);
            WebappActivityTestRule.waitUntilSplashHides(webappActivity);

            CriteriaHelper.pollInstrumentationThread(
                    () -> {
                        try {
                            String result =
                                    JavaScriptUtils.executeJavaScriptAndWaitForResult(
                                            webappTab.getWebContents(),
                                            "window.matchMedia('(display-mode:"
                                                    + " standalone)').matches");
                            Criteria.checkThat(result, Matchers.is("true"));
                        } catch (TimeoutException e) {
                            Criteria.checkThat("JS evaluation timed out", false, Matchers.is(true));
                        }
                    },
                    STARTUP_TIMEOUT,
                    CriteriaHelper.DEFAULT_POLLING_INTERVAL);

            String displayModeMatches =
                    JavaScriptUtils.executeJavaScriptAndWaitForResult(
                            webappTab.getWebContents(),
                            "window.matchMedia('(display-mode: standalone)').matches");
            Assert.assertEquals("true", displayModeMatches);

            int remainingTabCount =
                    ThreadUtils.runOnUiThreadBlocking(
                            () -> tabbedActivity.getTabModelSelector().getTotalTabCount());
            Assert.assertTrue(
                    "Originating browser window should not remain with zero tabs",
                    remainingTabCount > 0);
        } finally {
            webServer.shutdown();
        }
    }
}
