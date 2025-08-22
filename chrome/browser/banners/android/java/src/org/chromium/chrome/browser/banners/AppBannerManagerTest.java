// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.banners;

import static androidx.test.espresso.action.ViewActions.click;

import android.app.Activity;
import android.app.Instrumentation;
import android.app.Instrumentation.ActivityMonitor;
import android.app.Instrumentation.ActivityResult;
import android.content.Intent;
import android.content.IntentFilter;
import android.graphics.Bitmap;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

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
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.customtabs.CustomTabsIntentTestUtils;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.browser.TabLoadObserver;
import org.chromium.chrome.test.util.browser.TabTitleObserver;
import org.chromium.chrome.test.util.browser.webapps.WebappTestPage;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.webapps.AppBannerManager;
import org.chromium.components.webapps.AppData;
import org.chromium.components.webapps.AppDetailsDelegate;
import org.chromium.components.webapps.bottomsheet.PwaInstallBottomSheetView;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.ModalDialogProperties.ButtonType;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.ButtonCompat;

/** Tests the app banners. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class AppBannerManagerTest {
    @Rule
    public FreshCtaTransitTestRule mTabbedActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    @Rule
    public CustomTabActivityTestRule mCustomTabActivityTestRule = new CustomTabActivityTestRule();

    @Rule public ChromeBrowserTestRule mChromeBrowserTestRule = new ChromeBrowserTestRule();

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
    private WebPageStation mPage;

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
    private EmbeddedTestServer mTestServer;
    private UiDevice mUiDevice;
    private BottomSheetController mBottomSheetController;

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

        mPage = mTabbedActivityTestRule.startOnBlankPage();
        // Must be set after native has loaded.
        mDetailsDelegate = new MockAppDetailsDelegate();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AppBannerManager.setAppDetailsDelegate(mDetailsDelegate);
                });

        AppBannerManager.ignoreChromeChannelForTesting();
        AppBannerManager.setOverrideSegmentationResultForTesting(true);
        mTestServer =
                EmbeddedTestServer.createAndStartServer(
                        ApplicationProvider.getApplicationContext());
        mUiDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());

        mBottomSheetController =
                mTabbedActivityTestRule
                        .getActivity()
                        .getRootUiCoordinatorForTesting()
                        .getBottomSheetController();
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

    private void navigateToUrlAndWaitForBannerManager(
            ChromeActivityTestRule<? extends ChromeActivity> rule, String url) throws Exception {
        Tab tab = rule.getActivityTab();
        new TabLoadObserver(tab).fullyLoadUrl(url);
        waitForBannerManager(tab);
    }

    private void waitUntilAppDetailsRetrieved(
            ChromeActivityTestRule<? extends ChromeActivity> rule, final int numExpected) {
        CriteriaHelper.pollUiThread(
                () -> {
                    AppBannerManager manager =
                            getAppBannerManager(rule.getActivityTab().getWebContents());
                    Criteria.checkThat(mDetailsDelegate.mNumRetrieved, Matchers.is(numExpected));
                    Criteria.checkThat(manager.isRunningForTesting(), Matchers.is(false));
                });
    }

    private void checkPromotabilityStatus(
            ChromeActivityTestRule<? extends ChromeActivity> rule,
            final boolean isProbablyPromotable) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertEquals(
                            AppBannerManager.isProbablyPromotable(
                                    rule.getActivity().getActivityTab().getWebContents()),
                            isProbablyPromotable);
                });
    }

    private void waitUntilBottomSheetStatus(@BottomSheetController.SheetState int status) {
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(mBottomSheetController.getSheetState(), Matchers.is(status));
                });
    }

    private void waitUntilNoDialogsShowing(final Tab tab) throws Exception {
        UiObject dialogUiObject =
                mUiDevice.findObject(new UiSelector().text(EXPECTED_DIALOG_TITLE));
        dialogUiObject.waitUntilGone(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL);
    }

    private void tapAndWaitForModalBanner(final Tab tab) throws Exception {
        TouchCommon.singleClickView(tab.getView());

        UiObject dialogUiObject =
                mUiDevice.findObject(new UiSelector().text(EXPECTED_DIALOG_TITLE));
        Assert.assertTrue(dialogUiObject.waitForExists(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL));
    }

    private void triggerModalWebAppBanner(
            ChromeActivityTestRule<? extends ChromeActivity> rule, String url, boolean installApp)
            throws Exception {
        rule.loadUrlInNewTab(ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);
        navigateToUrlAndWaitForBannerManager(rule, url);

        Tab tab = rule.getActivityTab();
        tapAndWaitForModalBanner(tab);

        if (!installApp) return;

        // Click the button to trigger the adding of the shortcut.
        clickButton(rule.getActivity(), ButtonType.POSITIVE);
    }

    private void triggerModalNativeAppBanner(
            ChromeActivityTestRule<? extends ChromeActivity> rule,
            String url,
            String expectedReferrer,
            boolean installApp)
            throws Exception {
        rule.loadUrlInNewTab(ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);
        navigateToUrlAndWaitForBannerManager(rule, url);
        waitUntilAppDetailsRetrieved(rule, 1);
        Assert.assertEquals(expectedReferrer, mDetailsDelegate.mReferrer);

        final ChromeActivity activity = rule.getActivity();
        tapAndWaitForModalBanner(rule.getActivityTab());
        if (!installApp) return;

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

    private void triggerModalBannerMultipleTimes(
            ChromeActivityTestRule<? extends ChromeActivity> rule,
            String url,
            boolean isForNativeApp)
            throws Exception {
        rule.loadUrlInNewTab(ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);
        navigateToUrlAndWaitForBannerManager(rule, url);
        if (isForNativeApp) {
            waitUntilAppDetailsRetrieved(rule, 1);
        }

        Tab tab = rule.getActivityTab();
        tapAndWaitForModalBanner(tab);

        // Explicitly dismiss the banner. We should be able to show the banner after dismissing.
        clickButton(rule.getActivity(), ButtonType.NEGATIVE);
        waitUntilNoDialogsShowing(tab);
        tapAndWaitForModalBanner(tab);

        clickButton(rule.getActivity(), ButtonType.NEGATIVE);
        waitUntilNoDialogsShowing(tab);
        tapAndWaitForModalBanner(tab);
    }

    private void triggerBottomSheet(
            ChromeActivityTestRule<? extends ChromeActivity> rule, String url, boolean click)
            throws Exception {
        rule.loadUrlInNewTab(ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);
        navigateToUrlAndWaitForBannerManager(rule, url);

        if (click) {
            TouchCommon.singleClickView(rule.getActivityTab().getView());
            waitUntilBottomSheetStatus(BottomSheetController.SheetState.FULL);
            return;
        }

        waitUntilBottomSheetStatus(BottomSheetController.SheetState.PEEK);
    }

    private void clickButton(final ChromeActivity activity, @ButtonType final int buttonType) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PropertyModel model =
                            activity.getModalDialogManager().getCurrentDialogForTest();
                    model.get(ModalDialogProperties.CONTROLLER).onClick(model, buttonType);
                });
    }

    @Test
    @SmallTest
    @Feature({"AppBanners"})
    public void testAppInstalledEventModalWebAppBannerBrowserTab() throws Exception {
        triggerModalWebAppBanner(
                mTabbedActivityTestRule.getActivityTestRule(),
                WebappTestPage.getTestUrlWithAction(
                        mTestServer, "call_stashed_prompt_on_click_verify_appinstalled"),
                true);

        // The appinstalled event should fire (and cause the title to change).
        new TabTitleObserver(
                        mTabbedActivityTestRule.getActivityTab(),
                        "Got appinstalled: listener, attr")
                .waitForTitleUpdate(3);

        // In most test environments, the install action will fail and a shortcut
        // will be added instead. This does not affect the promotability status since
        // a new APK was not installed.
        checkPromotabilityStatus(mTabbedActivityTestRule.getActivityTestRule(), true);
        ThreadUtils.runOnUiThread(
                () -> {
                    Assert.assertEquals(
                            1,
                            RecordHistogram.getHistogramValueCountForTesting(
                                    "Webapp.Install.InstallEvent", 4 /* API_BROWSER_TAB */));

                    Assert.assertEquals(
                            1,
                            RecordHistogram.getHistogramValueCountForTesting(
                                    INSTALL_PATH_HISTOGRAM_NAME, /* sample= */ 3));
                });
    }

    @Test
    @SmallTest
    @Feature({"AppBanners"})
    public void testAppInstalledEventModalWebAppBannerCustomTab() throws Exception {
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(
                CustomTabsIntentTestUtils.createMinimalCustomTabIntent(
                        ApplicationProvider.getApplicationContext(),
                        ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL));
        triggerModalWebAppBanner(
                mCustomTabActivityTestRule,
                WebappTestPage.getTestUrlWithAction(
                        mTestServer, "call_stashed_prompt_on_click_verify_appinstalled"),
                true);

        // The appinstalled event should fire (and cause the title to change).
        new TabTitleObserver(
                        mCustomTabActivityTestRule.getActivityTab(),
                        "Got appinstalled: listener, attr")
                .waitForTitleUpdate(3);

        checkPromotabilityStatus(mCustomTabActivityTestRule, true);
        ThreadUtils.runOnUiThread(
                () -> {
                    Assert.assertEquals(
                            1,
                            RecordHistogram.getHistogramValueCountForTesting(
                                    "Webapp.Install.InstallEvent", 5 /* API_CUSTOM_TAB */));

                    Assert.assertEquals(
                            1,
                            RecordHistogram.getHistogramValueCountForTesting(
                                    INSTALL_PATH_HISTOGRAM_NAME, /* sample= */ 3));
                });
    }

    @Test
    @SmallTest
    @Feature({"AppBanners"})
    public void testAppInstalledModalNativeAppBannerBrowserTab() throws Exception {
        triggerModalNativeAppBanner(
                mTabbedActivityTestRule.getActivityTestRule(),
                WebappTestPage.getTestUrlWithManifestAndAction(
                        mTestServer,
                        NATIVE_APP_MANIFEST_WITH_ID,
                        "call_stashed_prompt_on_click_verify_appinstalled"),
                NATIVE_APP_BLANK_REFERRER,
                true);

        // The userChoice promise should resolve (and cause the title to change). appinstalled is
        // not fired for native apps
        new TabTitleObserver(mTabbedActivityTestRule.getActivityTab(), "Got userChoice: accepted")
                .waitForTitleUpdate(3);

        checkPromotabilityStatus(mTabbedActivityTestRule.getActivityTestRule(), false);
        Assert.assertEquals(
                0, RecordHistogram.getHistogramTotalCountForTesting(INSTALL_PATH_HISTOGRAM_NAME));
    }

    @Test
    @SmallTest
    @Feature({"AppBanners"})
    public void testAppInstalledModalNativeAppBannerBrowserTabWithUrl() throws Exception {
        triggerModalNativeAppBanner(
                mTabbedActivityTestRule.getActivityTestRule(),
                WebappTestPage.getTestUrlWithManifestAndAction(
                        mTestServer,
                        NATIVE_APP_MANIFEST_WITH_URL,
                        "call_stashed_prompt_on_click_verify_appinstalled"),
                NATIVE_APP_REFERRER,
                true);

        // The userChoice promise should resolve (and cause the title to change). appinstalled is
        // not fired for native apps
        new TabTitleObserver(mTabbedActivityTestRule.getActivityTab(), "Got userChoice: accepted")
                .waitForTitleUpdate(3);

        checkPromotabilityStatus(mTabbedActivityTestRule.getActivityTestRule(), false);
        Assert.assertEquals(
                0, RecordHistogram.getHistogramTotalCountForTesting(INSTALL_PATH_HISTOGRAM_NAME));
    }

    @Test
    @SmallTest
    @Feature({"AppBanners"})
    public void testAppInstalledModalNativeAppBannerCustomTab() throws Exception {
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(
                CustomTabsIntentTestUtils.createMinimalCustomTabIntent(
                        ApplicationProvider.getApplicationContext(),
                        ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL));

        triggerModalNativeAppBanner(
                mCustomTabActivityTestRule,
                WebappTestPage.getTestUrlWithManifestAndAction(
                        mTestServer,
                        NATIVE_APP_MANIFEST_WITH_ID,
                        "call_stashed_prompt_on_click_verify_appinstalled"),
                NATIVE_APP_BLANK_REFERRER,
                true);

        // The appinstalled event should fire (and cause the title to change).
        new TabTitleObserver(
                        mCustomTabActivityTestRule.getActivityTab(), "Got userChoice: accepted")
                .waitForTitleUpdate(3);

        checkPromotabilityStatus(mCustomTabActivityTestRule, false);
        Assert.assertEquals(
                0, RecordHistogram.getHistogramTotalCountForTesting(INSTALL_PATH_HISTOGRAM_NAME));
    }

    @Test
    @MediumTest
    @Feature({"AppBanners"})
    public void testBlockedModalWebAppBannerResolvesUserChoice() throws Exception {
        triggerModalWebAppBanner(
                mTabbedActivityTestRule.getActivityTestRule(),
                WebappTestPage.getTestUrlWithAction(mTestServer, "call_stashed_prompt_on_click"),
                false);

        // Explicitly dismiss the banner.
        final ChromeActivity activity = mTabbedActivityTestRule.getActivity();
        clickButton(activity, ButtonType.NEGATIVE);

        // Ensure userChoice is resolved.
        new TabTitleObserver(mTabbedActivityTestRule.getActivityTab(), "Got userChoice: dismissed")
                .waitForTitleUpdate(3);

        checkPromotabilityStatus(mTabbedActivityTestRule.getActivityTestRule(), true);
        Assert.assertEquals(
                0, RecordHistogram.getHistogramTotalCountForTesting(INSTALL_PATH_HISTOGRAM_NAME));
    }

    @Test
    @MediumTest
    @Feature({"AppBanners"})
    public void testBlockedModalNativeAppBannerResolveUserChoice() throws Exception {
        triggerModalNativeAppBanner(
                mTabbedActivityTestRule.getActivityTestRule(),
                WebappTestPage.getTestUrlWithManifestAndAction(
                        mTestServer, NATIVE_APP_MANIFEST_WITH_ID, "call_stashed_prompt_on_click"),
                NATIVE_APP_BLANK_REFERRER,
                false);

        // Explicitly dismiss the banner.
        final ChromeActivity activity = mTabbedActivityTestRule.getActivity();
        clickButton(activity, ButtonType.NEGATIVE);

        // Ensure userChoice is resolved.
        new TabTitleObserver(mTabbedActivityTestRule.getActivityTab(), "Got userChoice: dismissed")
                .waitForTitleUpdate(3);

        checkPromotabilityStatus(mTabbedActivityTestRule.getActivityTestRule(), false);
        Assert.assertEquals(
                0, RecordHistogram.getHistogramTotalCountForTesting(INSTALL_PATH_HISTOGRAM_NAME));
    }

    @Test
    @MediumTest
    @Feature({"AppBanners"})
    public void testModalNativeAppBannerCanBeTriggeredMultipleTimesBrowserTab() throws Exception {
        triggerModalBannerMultipleTimes(
                mTabbedActivityTestRule.getActivityTestRule(),
                WebappTestPage.getTestUrlWithManifestAndAction(
                        mTestServer, NATIVE_APP_MANIFEST_WITH_ID, "call_stashed_prompt_on_click"),
                true);

        Assert.assertEquals(
                0, RecordHistogram.getHistogramTotalCountForTesting(INSTALL_PATH_HISTOGRAM_NAME));
    }

    @Test
    @MediumTest
    @Feature({"AppBanners"})
    public void testModalNativeAppBannerCanBeTriggeredMultipleTimesCustomTab() throws Exception {
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(
                CustomTabsIntentTestUtils.createMinimalCustomTabIntent(
                        ApplicationProvider.getApplicationContext(),
                        ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL));

        triggerModalBannerMultipleTimes(
                mCustomTabActivityTestRule,
                WebappTestPage.getTestUrlWithManifestAndAction(
                        mTestServer, NATIVE_APP_MANIFEST_WITH_ID, "call_stashed_prompt_on_click"),
                true);

        Assert.assertEquals(
                0, RecordHistogram.getHistogramTotalCountForTesting(INSTALL_PATH_HISTOGRAM_NAME));
    }

    @Test
    @MediumTest
    @Feature({"AppBanners"})
    public void testModalWebAppBannerCanBeTriggeredMultipleTimesBrowserTab() throws Exception {
        triggerModalBannerMultipleTimes(
                mTabbedActivityTestRule.getActivityTestRule(),
                WebappTestPage.getTestUrlWithAction(mTestServer, "call_stashed_prompt_on_click"),
                false);

        Assert.assertEquals(
                0, RecordHistogram.getHistogramTotalCountForTesting(INSTALL_PATH_HISTOGRAM_NAME));
    }

    @Test
    @SmallTest
    @Feature({"AppBanners"})
    public void testModalWebAppBannerCanBeTriggeredMultipleTimesCustomTab() throws Exception {
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(
                CustomTabsIntentTestUtils.createMinimalCustomTabIntent(
                        ApplicationProvider.getApplicationContext(),
                        ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL));

        triggerModalBannerMultipleTimes(
                mCustomTabActivityTestRule,
                WebappTestPage.getTestUrlWithAction(mTestServer, "call_stashed_prompt_on_click"),
                false);

        Assert.assertEquals(
                0, RecordHistogram.getHistogramTotalCountForTesting(INSTALL_PATH_HISTOGRAM_NAME));
    }

    @Test
    @SmallTest
    @Feature({"AppBanners"})
    public void testModalWebAppBannerTriggeredWithUnsupportedNativeApp() throws Exception {
        // The web app banner should show if preferred_related_applications is true but there is no
        // supported application platform specified in the related applications list.
        triggerModalWebAppBanner(
                mTabbedActivityTestRule.getActivityTestRule(),
                WebappTestPage.getTestUrlWithManifestAndAction(
                        mTestServer,
                        WEB_APP_MANIFEST_WITH_UNSUPPORTED_PLATFORM,
                        "call_stashed_prompt_on_click"),
                false);

        Assert.assertEquals(
                0, RecordHistogram.getHistogramTotalCountForTesting(INSTALL_PATH_HISTOGRAM_NAME));
    }

    @Test
    @SmallTest
    @Feature({"AppBanners"})
    public void testBottomSheet() throws Exception {
        triggerBottomSheet(
                mTabbedActivityTestRule.getActivityTestRule(),
                WebappTestPage.getTestUrlWithManifest(
                        mTestServer, WEB_APP_MANIFEST_FOR_BOTTOM_SHEET_INSTALL),
                /* click= */ false);

        View content = mBottomSheetController.getCurrentSheetContent().getContentView();

        // Expand the bottom sheet via drag handle.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ImageView dragHandle = content.findViewById(R.id.drag_handlebar);
                    TouchCommon.singleClickView(dragHandle);
                });

        waitUntilBottomSheetStatus(BottomSheetController.SheetState.FULL);

        TextView appName =
                content.findViewById(PwaInstallBottomSheetView.getAppNameViewIdForTesting());
        TextView appOrigin =
                content.findViewById(PwaInstallBottomSheetView.getAppOriginViewIdForTesting());
        TextView description =
                content.findViewById(PwaInstallBottomSheetView.getDescViewIdForTesting());

        Assert.assertEquals("PWA Bottom Sheet", appName.getText());
        Assert.assertTrue(appOrigin.getText().toString().startsWith("http://127.0.0.1:"));
        Assert.assertEquals(
                "Lorem ipsum dolor sit amet, consectetur adipiscing elit, "
                        + "sed do eiusmod tempor incididunt ut labore et dolore magna aliqua",
                description.getText());

        // Collapse the bottom sheet.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ImageView dragHandle = content.findViewById(R.id.drag_handlebar);
                    TouchCommon.singleClickView(dragHandle);
                });

        waitUntilBottomSheetStatus(BottomSheetController.SheetState.PEEK);

        // Dismiss the bottom sheet.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mBottomSheetController.hideContent(
                            mBottomSheetController.getCurrentSheetContent(), false);
                });

        waitUntilBottomSheetStatus(BottomSheetController.SheetState.HIDDEN);

        Assert.assertEquals(
                0, RecordHistogram.getHistogramTotalCountForTesting(INSTALL_PATH_HISTOGRAM_NAME));
    }

    @Test
    @MediumTest
    @Feature({"AppBanners"})
    public void testAppInstalledEventBottomSheet() throws Exception {
        triggerBottomSheet(
                mTabbedActivityTestRule.getActivityTestRule(),
                WebappTestPage.getTestUrlWithManifestAndAction(
                        mTestServer,
                        WEB_APP_MANIFEST_FOR_BOTTOM_SHEET_INSTALL,
                        "call_stashed_prompt_on_click_verify_appinstalled"),
                /* click= */ true);

        View content = mBottomSheetController.getCurrentSheetContent().getContentView();

        // Install app from the bottom sheet.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ButtonCompat buttonInstall =
                            content.findViewById(
                                    PwaInstallBottomSheetView.getButtonInstallViewIdForTesting());
                    TouchCommon.singleClickView(buttonInstall);
                });

        waitUntilBottomSheetStatus(BottomSheetController.SheetState.HIDDEN);

        // The appinstalled event should fire (and cause the title to change).
        new TabTitleObserver(
                        mTabbedActivityTestRule.getActivityTab(),
                        "Got appinstalled: listener, attr")
                .waitForTitleUpdate(3);

        ThreadUtils.runOnUiThread(
                () -> {
                    Assert.assertEquals(
                            1,
                            RecordHistogram.getHistogramValueCountForTesting(
                                    "Webapp.Install.InstallEvent", 4 /* API_BROWSER_TAB */));

                    Assert.assertEquals(
                            1,
                            RecordHistogram.getHistogramValueCountForTesting(
                                    INSTALL_PATH_HISTOGRAM_NAME, /* sample= */ 6));
                });
    }

    @Test
    @MediumTest
    @Feature({"AppBanners"})
    public void testDismissBottomSheetResolvesUserChoice() throws Exception {
        triggerBottomSheet(
                mTabbedActivityTestRule.getActivityTestRule(),
                WebappTestPage.getTestUrlWithManifestAndAction(
                        mTestServer,
                        WEB_APP_MANIFEST_FOR_BOTTOM_SHEET_INSTALL,
                        "call_stashed_prompt_on_click"),
                /* click= */ true);

        // Dismiss the bottom sheet.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mBottomSheetController.hideContent(
                            mBottomSheetController.getCurrentSheetContent(), false);
                });

        waitUntilBottomSheetStatus(BottomSheetController.SheetState.HIDDEN);

        // Ensure userChoice is resolved.
        new TabTitleObserver(mTabbedActivityTestRule.getActivityTab(), "Got userChoice: dismissed")
                .waitForTitleUpdate(3);

        Assert.assertEquals(
                0, RecordHistogram.getHistogramTotalCountForTesting(INSTALL_PATH_HISTOGRAM_NAME));
    }

    @Test
    @MediumTest
    @Feature({"AppBanners"})
    public void testBlockedBottomSheetDoesNotAppearAgainForMonths() throws Exception {
        String url =
                WebappTestPage.getTestUrlWithManifestAndAction(
                        mTestServer,
                        WEB_APP_MANIFEST_FOR_BOTTOM_SHEET_INSTALL,
                        "call_stashed_prompt_on_click");
        triggerBottomSheet(mTabbedActivityTestRule.getActivityTestRule(), url, /* click= */ true);

        // Dismiss the bottom sheet after expanding it.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mBottomSheetController.hideContent(
                            mBottomSheetController.getCurrentSheetContent(), false);
                });
        waitUntilBottomSheetStatus(BottomSheetController.SheetState.HIDDEN);

        // Waiting two months shouldn't be long enough.
        AppBannerManager.setTimeDeltaForTesting(61);
        navigateToUrlAndWaitForBannerManager(mTabbedActivityTestRule.getActivityTestRule(), url);
        waitUntilBottomSheetStatus(BottomSheetController.SheetState.HIDDEN);

        AppBannerManager.setTimeDeltaForTesting(62);
        navigateToUrlAndWaitForBannerManager(mTabbedActivityTestRule.getActivityTestRule(), url);
        waitUntilBottomSheetStatus(BottomSheetController.SheetState.HIDDEN);

        // Waiting three months should allow the bottom sheet to reappear.
        AppBannerManager.setTimeDeltaForTesting(91);
        navigateToUrlAndWaitForBannerManager(mTabbedActivityTestRule.getActivityTestRule(), url);
        waitUntilBottomSheetStatus(BottomSheetController.SheetState.PEEK);
    }

    @Test
    @MediumTest
    @Feature({"AppBanners"})
    public void testBottomSheetSkipsHiddenWebContents() throws Exception {
        String url =
                WebappTestPage.getTestUrlWithManifestAndAction(
                        mTestServer,
                        WEB_APP_MANIFEST_FOR_BOTTOM_SHEET_INSTALL,
                        "call_stashed_prompt_on_click");

        mTabbedActivityTestRule.loadUrl(ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);

        // Create an extra tab so that there is a background tab.
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(),
                mTabbedActivityTestRule.getActivity(),
                /* incognito= */ false,
                /* waitForNtpLoad= */ true);

        Tab backgroundTab =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                mTabbedActivityTestRule
                                        .getActivity()
                                        .getCurrentTabModel()
                                        .getTabAt(0));
        Assert.assertTrue(backgroundTab != null);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    backgroundTab.loadUrl(new LoadUrlParams(url));
                });

        waitForAppBannerPipelineStatus(
                backgroundTab, AppBannerManagerState.PENDING_PROMPT_NOT_CANCELED);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertEquals(
                            BottomSheetController.SheetState.HIDDEN,
                            mBottomSheetController.getSheetState());
                });
    }

    @Test
    @MediumTest
    @Feature({"AppBanners"})
    public void testAppBannerDismissedAfterNavigation() throws Exception {
        String url =
                WebappTestPage.getTestUrlWithAction(mTestServer, "call_stashed_prompt_on_click");

        mTabbedActivityTestRule.loadUrlInNewTab(ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);
        navigateToUrlAndWaitForBannerManager(mTabbedActivityTestRule.getActivityTestRule(), url);

        Tab tab = mTabbedActivityTestRule.getActivityTab();
        tapAndWaitForModalBanner(tab);

        // Navigate and check that the dialog was dismissed.
        mTabbedActivityTestRule.loadUrl(ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);
        waitUntilNoDialogsShowing(tab);
    }
}
