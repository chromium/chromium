// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.banners;

import android.app.Activity;
import android.app.Instrumentation;
import android.app.Instrumentation.ActivityMonitor;
import android.app.Instrumentation.ActivityResult;
import android.content.Context;
import android.content.ContextWrapper;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.graphics.Bitmap;
import android.text.TextUtils;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.uiautomator.UiDevice;
import androidx.test.uiautomator.UiObject;
import androidx.test.uiautomator.UiSelector;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.PackageManagerWrapper;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.customtabs.CustomTabsIntentTestUtils;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.TabLoadObserver;
import org.chromium.chrome.test.util.browser.TabTitleObserver;
import org.chromium.chrome.test.util.browser.webapps.WebappTestPage;
import org.chromium.components.messages.DismissReason;
import org.chromium.components.messages.MessageBannerProperties;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.messages.MessageDispatcherProvider;
import org.chromium.components.messages.MessageIdentifier;
import org.chromium.components.messages.MessagesTestHelper;
import org.chromium.components.site_engagement.SiteEngagementService;
import org.chromium.components.webapps.AppBannerManager;
import org.chromium.components.webapps.AppData;
import org.chromium.components.webapps.AppDetailsDelegate;
import org.chromium.components.webapps.WebappInstallSource;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.ModalDialogProperties.ButtonType;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.Observer;

/** Tests the app banners. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class AmbientBadgeManagerTest {
    @Rule
    public ChromeTabbedActivityTestRule mTabbedActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public CustomTabActivityTestRule mCustomTabActivityTestRule = new CustomTabActivityTestRule();

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    // The ID of the last event received.
    private String mLastNotifyEvent;

    private static final String NATIVE_APP_MANIFEST_WITH_ID =
            "/chrome/test/data/banners/play_app_manifest.json";

    private static final String NATIVE_APP_MANIFEST_WITH_URL =
            "/chrome/test/data/banners/play_app_url_manifest.json";

    private static final String WEB_APP_MANIFEST_WITH_UNSUPPORTED_PLATFORM =
            "/chrome/test/data/banners/manifest_prefer_related_chrome_app.json";

    private static final String WEB_APP_MANIFEST_WITH_RELATED_APP_LIST =
            "/chrome/test/data/banners/manifest_listing_related_android_app.json";

    private static final String WEB_APP_MANIFEST_FOR_BOTTOM_SHEET_INSTALL =
            "/chrome/test/data/banners/manifest_with_screenshots.json";

    private static final String NATIVE_ICON_PATH = "/chrome/test/data/banners/launcher-icon-4x.png";

    private static final String NATIVE_APP_TITLE = "Mock app title";

    private static final String NATIVE_APP_INSTALL_TEXT = "Install this";

    private static final String NATIVE_APP_REFERRER = "chrome_inline&playinline=chrome_inline";

    private static final String NATIVE_APP_BLANK_REFERRER = "playinline=chrome_inline";

    private static final String NATIVE_APP_PACKAGE_NAME = "com.example.app";

    private static final String INSTALL_ACTION = "INSTALL_ACTION";

    private static final String INSTALL_PATH_HISTOGRAM_NAME = "WebApk.Install.PathToInstall";

    private static final String EXPECTED_DIALOG_TITLE = "Install app";

    private class MockAppDetailsDelegate extends AppDetailsDelegate {
        private Observer mObserver;
        private AppData mAppData;
        private int mNumRetrieved;
        private Intent mInstallIntent;
        private String mReferrer;

        @Override
        public void getAppDetailsAsynchronously(
                Observer observer, String url, String packageName, String referrer, int iconSize) {
            mNumRetrieved += 1;
            mObserver = observer;
            mReferrer = referrer;
            mInstallIntent = new Intent(INSTALL_ACTION);

            mAppData = new AppData(url, packageName);
            mAppData.setPackageInfo(
                    NATIVE_APP_TITLE,
                    mTestServer.getURL(NATIVE_ICON_PATH),
                    4.5f,
                    NATIVE_APP_INSTALL_TEXT,
                    null,
                    mInstallIntent);
            PostTask.runOrPostTask(
                    TaskTraits.UI_DEFAULT,
                    () -> {
                        mObserver.onAppDetailsRetrieved(mAppData);
                    });
        }

        @Override
        public void destroy() {}
    }

    private MockAppDetailsDelegate mDetailsDelegate;
    @Mock private PackageManager mPackageManager;
    private EmbeddedTestServer mTestServer;
    private UiDevice mUiDevice;

    @Before
    public void setUp() throws Exception {
        AppBannerManager.setIsSupported(true);
        ShortcutHelper.setDelegateForTests(
                new ShortcutHelper.Delegate() {
                    @Override
                    public void addShortcutToHomescreen(
                            String id,
                            String title,
                            Bitmap icon,
                            boolean iconAdaptive,
                            Intent shortcutIntent) {
                        // Ignore to prevent adding homescreen shortcuts.
                    }
                });

        mTabbedActivityTestRule.startMainActivityOnBlankPage();
        // Must be set after native has loaded.
        mDetailsDelegate = new MockAppDetailsDelegate();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AppBannerManager.setAppDetailsDelegate(mDetailsDelegate);
                });

        AppBannerManager.ignoreChromeChannelForTesting();
        AppBannerManager.setTotalEngagementForTesting(10);
        AppBannerManager.setOverrideSegmentationResultForTesting(true);
        mTestServer =
                EmbeddedTestServer.createAndStartServer(
                        ApplicationProvider.getApplicationContext());
        mUiDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());
    }

    private void resetEngagementForUrl(final String url, final double engagement) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // TODO (https://crbug.com/1063807):  Add incognito mode tests.
                    SiteEngagementService.getForBrowserContext(
                                    ProfileManager.getLastUsedRegularProfile())
                            .resetBaseScoreForUrl(url, engagement);
                });
    }

    private AppBannerManager getAppBannerManager(WebContents webContents) {
        return AppBannerManager.forWebContents(webContents);
    }

    private void waitForBannerManager(Tab tab) {
        CriteriaHelper.pollUiThread(
                () -> !getAppBannerManager(tab.getWebContents()).isRunningForTesting());
    }

    private void waitForAppBannerPipelineStatus(Tab tab, int expectedValue) {
        CriteriaHelper.pollUiThread(
                () -> {
                    return getAppBannerManager(tab.getWebContents()).getPipelineStatusForTesting()
                            == expectedValue;
                });
    }

    private void assertAppBannerPipelineStatus(int expectedValue) {
        Tab tab = mTabbedActivityTestRule.getActivity().getActivityTab();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertEquals(
                            expectedValue,
                            getAppBannerManager(tab.getWebContents())
                                    .getPipelineStatusForTesting());
                });
    }

    private void navigateToUrlAndWaitForBannerManager(
            ChromeActivityTestRule<? extends ChromeActivity> rule, String url) throws Exception {
        Tab tab = rule.getActivity().getActivityTab();
        new TabLoadObserver(tab).fullyLoadUrl(url);
        waitForBannerManager(tab);
    }

    private void waitUntilAppDetailsRetrieved(
            ChromeActivityTestRule<? extends ChromeActivity> rule, final int numExpected) {
        CriteriaHelper.pollUiThread(
                () -> {
                    AppBannerManager manager =
                            getAppBannerManager(
                                    rule.getActivity().getActivityTab().getWebContents());
                    Criteria.checkThat(mDetailsDelegate.mNumRetrieved, Matchers.is(numExpected));
                    Criteria.checkThat(manager.isRunningForTesting(), Matchers.is(false));
                });
    }

    private void waitUntilAmbientBadgePromptAppears(
            ChromeActivityTestRule<? extends ChromeActivity> rule) {
        WindowAndroid windowAndroid = rule.getActivity().getWindowAndroid();
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            MessagesTestHelper.getMessageCount(windowAndroid), Matchers.is(1));
                    Criteria.checkThat(
                            MessagesTestHelper.getMessageIdentifier(windowAndroid, 0),
                            Matchers.is(MessageIdentifier.INSTALLABLE_AMBIENT_BADGE));
                });
    }

    private void checkAmbientBadgePromptNotExist(
            ChromeActivityTestRule<? extends ChromeActivity> rule) {
        WindowAndroid windowAndroid = rule.getActivity().getWindowAndroid();
        ThreadUtils.runOnUiThreadBlocking(
                () -> Assert.assertEquals(0, MessagesTestHelper.getMessageCount(windowAndroid)));
    }

    private void waitForBadgeStatus(Tab tab, int expectedValue) {
        CriteriaHelper.pollUiThread(
                () -> {
                    return getAppBannerManager(tab.getWebContents()).getBadgeStatusForTesting()
                            == expectedValue;
                });
    }

    private void waitForModalBanner(final ChromeActivity activity) throws Exception {
        UiObject dialogUiObject =
                mUiDevice.findObject(new UiSelector().text(EXPECTED_DIALOG_TITLE));
        Assert.assertTrue(dialogUiObject.waitForExists(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL));
    }

    private void triggerInstallWebApp(
            ChromeActivityTestRule<? extends ChromeActivity> rule, String url) throws Exception {
        resetEngagementForUrl(url, 10);
        rule.loadUrlInNewTab(ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);
        navigateToUrlAndWaitForBannerManager(rule, url);
        waitUntilAmbientBadgePromptAppears(rule);

        dismissAmbientBadgeMessage(rule, true);
        waitForModalBanner(rule.getActivity());

        // Click the button to trigger the adding of the shortcut.
        clickButton(rule.getActivity(), ButtonType.POSITIVE);
    }

    private void triggerInstallNative(
            ChromeActivityTestRule<? extends ChromeActivity> rule,
            String url,
            String expectedReferrer)
            throws Exception {
        resetEngagementForUrl(url, 10);
        rule.loadUrlInNewTab(ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);
        navigateToUrlAndWaitForBannerManager(rule, url);
        waitUntilAppDetailsRetrieved(rule, 1);
        waitUntilAmbientBadgePromptAppears(rule);
        Assert.assertEquals(mDetailsDelegate.mReferrer, expectedReferrer);

        dismissAmbientBadgeMessage(rule, true);
        final ChromeActivity activity = rule.getActivity();
        waitForModalBanner(activity);

        // Click the button to trigger the installation.
        final ActivityMonitor activityMonitor =
                new ActivityMonitor(
                        new IntentFilter(INSTALL_ACTION),
                        new ActivityResult(Activity.RESULT_OK, null),
                        true);
        Instrumentation instrumentation = InstrumentationRegistry.getInstrumentation();
        instrumentation.addMonitor(activityMonitor);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    String buttonText =
                            activity.getModalDialogManager()
                                    .getCurrentDialogForTest()
                                    .get(ModalDialogProperties.POSITIVE_BUTTON_TEXT);
                    Assert.assertEquals(NATIVE_APP_INSTALL_TEXT, buttonText);
                });

        clickButton(activity, ButtonType.POSITIVE);

        // Wait until the installation triggers.
        instrumentation.waitForMonitorWithTimeout(
                activityMonitor, CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL);
    }

    private void clickButton(final ChromeActivity activity, @ButtonType final int buttonType) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PropertyModel model =
                            activity.getModalDialogManager().getCurrentDialogForTest();
                    model.get(ModalDialogProperties.CONTROLLER).onClick(model, buttonType);
                });
    }

    private void dismissAmbientBadgeMessage(
            ChromeActivityTestRule<? extends ChromeActivity> rule, boolean accept)
            throws Exception {
        WindowAndroid windowAndroid = rule.getActivity().getWindowAndroid();

        MessageDispatcher dispatcher =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> MessageDispatcherProvider.from(windowAndroid));
        PropertyModel model =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                MessagesTestHelper.getCurrentMessage(
                                        MessagesTestHelper.getEnqueuedMessages(
                                                        dispatcher,
                                                        MessageIdentifier.INSTALLABLE_AMBIENT_BADGE)
                                                .get(0)));
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    if (accept) {
                        model.get(MessageBannerProperties.ON_PRIMARY_ACTION).get();
                    } else {
                        dispatcher.dismissMessage(model, DismissReason.GESTURE);
                    }
                });
    }

    @Test
    @SmallTest
    public void testAmbientBadgeInstalledWebAppBrowserTab() throws Exception {
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Webapp.Install.InstallEvent",
                                WebappInstallSource.AMBIENT_BADGE_BROWSER_TAB)
                        .expectIntRecord(INSTALL_PATH_HISTOGRAM_NAME, 1 /* kAmbientInfobar */)
                        .build();

        triggerInstallWebApp(
                mTabbedActivityTestRule,
                WebappTestPage.getNonServiceWorkerUrlWithAction(
                        mTestServer, "verify_appinstalled"));

        // The appinstalled event should fire (and cause the title to change).
        new TabTitleObserver(
                        mTabbedActivityTestRule.getActivity().getActivityTab(),
                        "Got appinstalled: listener, attr")
                .waitForTitleUpdate(3);

        watcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testAmbientBadgeInstalledWebAppCustomTab() throws Exception {
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Webapp.Install.InstallEvent",
                                WebappInstallSource.AMBIENT_BADGE_CUSTOM_TAB)
                        .expectIntRecord(INSTALL_PATH_HISTOGRAM_NAME, 1 /* kAmbientInfobar */)
                        .build();

        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(
                CustomTabsIntentTestUtils.createMinimalCustomTabIntent(
                        ApplicationProvider.getApplicationContext(),
                        ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL));
        triggerInstallWebApp(
                mCustomTabActivityTestRule,
                WebappTestPage.getNonServiceWorkerUrlWithAction(
                        mTestServer, "verify_appinstalled"));

        // The appinstalled event should fire (and cause the title to change).
        new TabTitleObserver(
                        mCustomTabActivityTestRule.getActivity().getActivityTab(),
                        "Got appinstalled: listener, attr")
                .waitForTitleUpdate(3);

        watcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testAmbientBadgeInstalledNativeAppBrowserTab() throws Exception {
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder().expectNoRecords(INSTALL_PATH_HISTOGRAM_NAME).build();

        triggerInstallNative(
                mTabbedActivityTestRule,
                WebappTestPage.getNonServiceWorkerUrlWithManifest(
                        mTestServer, NATIVE_APP_MANIFEST_WITH_ID),
                NATIVE_APP_BLANK_REFERRER);

        watcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testAmbientBadgeInstalledNativeAppCustomTab() throws Exception {
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder().expectNoRecords(INSTALL_PATH_HISTOGRAM_NAME).build();

        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(
                CustomTabsIntentTestUtils.createMinimalCustomTabIntent(
                        ApplicationProvider.getApplicationContext(),
                        ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL));

        triggerInstallNative(
                mCustomTabActivityTestRule,
                WebappTestPage.getNonServiceWorkerUrlWithManifest(
                        mTestServer, NATIVE_APP_MANIFEST_WITH_ID),
                NATIVE_APP_BLANK_REFERRER);

        watcher.assertExpected();
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({"bypass-installable-message-throttle-for-testing"})
    public void testBlockedAmbientBadgeDoesNotAppearAgainForMonths() throws Exception {
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder().expectNoRecords(INSTALL_PATH_HISTOGRAM_NAME).build();

        // Visit a site that is a PWA. The ambient badge should show.
        String webBannerUrl = WebappTestPage.getNonServiceWorkerUrl(mTestServer);
        resetEngagementForUrl(webBannerUrl, 10);
        Tab tab = mTabbedActivityTestRule.getActivity().getActivityTab();
        new TabLoadObserver(tab).fullyLoadUrl(webBannerUrl);
        waitUntilAmbientBadgePromptAppears(mTabbedActivityTestRule);

        WindowAndroid windowAndroid = mTabbedActivityTestRule.getActivity().getWindowAndroid();

        // Explicitly dismiss the ambient badge.
        CriteriaHelper.pollUiThread(
                () ->
                        Criteria.checkThat(
                                MessagesTestHelper.getMessageCount(windowAndroid), Matchers.is(1)));

        MessageDispatcher dispatcher =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> MessageDispatcherProvider.from(windowAndroid));
        PropertyModel model =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                MessagesTestHelper.getCurrentMessage(
                                        MessagesTestHelper.getEnqueuedMessages(
                                                        dispatcher,
                                                        MessageIdentifier.INSTALLABLE_AMBIENT_BADGE)
                                                .get(0)));
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    dispatcher.dismissMessage(model, DismissReason.GESTURE);
                });
        CriteriaHelper.pollUiThread(
                () ->
                        Criteria.checkThat(
                                MessagesTestHelper.getMessageCount(windowAndroid), Matchers.is(0)));

        // Waiting two months shouldn't be long enough.
        AppBannerManager.setTimeDeltaForTesting(61);
        new TabLoadObserver(tab).fullyLoadUrl(webBannerUrl);
        CriteriaHelper.pollUiThread(
                () ->
                        Criteria.checkThat(
                                MessagesTestHelper.getMessageCount(windowAndroid), Matchers.is(0)));

        AppBannerManager.setTimeDeltaForTesting(62);
        new TabLoadObserver(tab).fullyLoadUrl(webBannerUrl);
        CriteriaHelper.pollUiThread(
                () ->
                        Criteria.checkThat(
                                MessagesTestHelper.getMessageCount(windowAndroid), Matchers.is(0)));

        // Waiting three months should allow the ambient badge to reappear.
        AppBannerManager.setTimeDeltaForTesting(91);
        new TabLoadObserver(tab).fullyLoadUrl(webBannerUrl);
        CriteriaHelper.pollUiThread(
                () ->
                        Criteria.checkThat(
                                MessagesTestHelper.getMessageCount(windowAndroid), Matchers.is(1)));

        watcher.assertExpected();
    }

    @Test
    @MediumTest
    public void testAmbientBadgeAppearWithServiceWorkerPage() throws Exception {
        String webBannerUrl = WebappTestPage.getNonServiceWorkerUrl(mTestServer);
        resetEngagementForUrl(webBannerUrl, 10);
        navigateToUrlAndWaitForBannerManager(mTabbedActivityTestRule, webBannerUrl);

        Tab tab = mTabbedActivityTestRule.getActivity().getActivityTab();
        waitForBadgeStatus(tab, AmbientBadgeState.SHOWING);
        waitUntilAmbientBadgePromptAppears(mTabbedActivityTestRule);
    }

    @Test
    @SmallTest
    public void testAmbientBadgeTriggeredWithListedRelatedApp() throws Exception {
        // The ambient badge should show if there is play app in related applications list but
        // preferred_related_applications is false.
        String webBannerUrl = WebappTestPage.getNonServiceWorkerUrl(mTestServer);
        resetEngagementForUrl(webBannerUrl, 10);
        navigateToUrlAndWaitForBannerManager(mTabbedActivityTestRule, webBannerUrl);

        waitUntilAmbientBadgePromptAppears(mTabbedActivityTestRule);
    }

    private static class TestContext extends ContextWrapper {
        public TestContext(Context baseContext) {
            super(baseContext);
        }

        @Override
        public PackageManager getPackageManager() {
            return new PackageManagerWrapper(super.getPackageManager()) {
                @Override
                public PackageInfo getPackageInfo(String packageName, int flags)
                        throws PackageManager.NameNotFoundException {
                    if (TextUtils.equals(NATIVE_APP_PACKAGE_NAME, packageName)) {
                        PackageInfo packageInfo = new PackageInfo();
                        packageInfo.packageName = NATIVE_APP_PACKAGE_NAME;
                        return packageInfo;
                    }

                    return TestContext.super.getPackageManager().getPackageInfo(packageName, flags);
                }
            };
        }
    }

    @Test
    @SmallTest
    public void testAmbientBadgeDoesNotAppearWhenRelatedAppInstalled() throws Exception {
        String url =
                WebappTestPage.getNonServiceWorkerUrlWithManifest(
                        mTestServer, WEB_APP_MANIFEST_WITH_RELATED_APP_LIST);
        resetEngagementForUrl(url, 10);

        final Context contextToRestore = ContextUtils.getApplicationContext();
        ContextUtils.initApplicationContextForTests(new TestContext(contextToRestore));

        navigateToUrlAndWaitForBannerManager(mTabbedActivityTestRule, url);

        assertAppBannerPipelineStatus(AppBannerManagerState.COMPLETE);

        // The web app banner not show if a play app in related applications list is installed.
        checkAmbientBadgePromptNotExist(mTabbedActivityTestRule);

        ContextUtils.initApplicationContextForTests(contextToRestore);
    }

    @Test
    @SmallTest
    public void testMlShowAmbientBadge() throws Exception {
        String url = WebappTestPage.getNonServiceWorkerUrl(mTestServer);
        resetEngagementForUrl(url, 10);
        AppBannerManager.setOverrideSegmentationResultForTesting(false);

        navigateToUrlAndWaitForBannerManager(mTabbedActivityTestRule, url);

        assertAppBannerPipelineStatus(AppBannerManagerState.PENDING_PROMPT_NOT_CANCELED);

        Tab tab = mTabbedActivityTestRule.getActivity().getActivityTab();

        // Blocked by segmentation result.
        waitForBadgeStatus(tab, AmbientBadgeState.SEGMENTATION_BLOCK);
        checkAmbientBadgePromptNotExist(mTabbedActivityTestRule);

        // Advance 3 days and navigate to |url| again
        AppBannerManager.setTimeDeltaForTesting(3);
        AppBannerManager.setOverrideSegmentationResultForTesting(true);
        mTabbedActivityTestRule.loadUrl(ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);
        navigateToUrlAndWaitForBannerManager(mTabbedActivityTestRule, url);

        waitForBadgeStatus(tab, AmbientBadgeState.SHOWING);
        waitUntilAmbientBadgePromptAppears(mTabbedActivityTestRule);
    }
}
