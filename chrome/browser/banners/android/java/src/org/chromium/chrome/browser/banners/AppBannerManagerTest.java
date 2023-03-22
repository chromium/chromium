// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.banners;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.assertThat;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isRoot;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.is;
import static org.hamcrest.Matchers.not;

import static org.chromium.ui.test.util.ViewUtils.VIEW_NULL;
import static org.chromium.ui.test.util.ViewUtils.waitForView;

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
import android.support.test.InstrumentationRegistry;
import android.text.TextUtils;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.test.espresso.ViewInteraction;
import androidx.test.espresso.matcher.RootMatchers;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;
import androidx.test.uiautomator.UiDevice;
import androidx.test.uiautomator.UiObject;
import androidx.test.uiautomator.UiSelector;

import org.hamcrest.Matcher;
import org.hamcrest.Matchers;
import org.junit.After;
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
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.PackageManagerWrapper;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.browserservices.intents.BitmapHelper;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.customtabs.CustomTabsIntentTestUtils;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.infobar.InfoBarContainer;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabUtils;
import org.chromium.chrome.browser.ui.appmenu.AppMenuCoordinator;
import org.chromium.chrome.browser.ui.appmenu.AppMenuTestSupport;
import org.chromium.chrome.browser.webapps.WebappDataStorage;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.InfoBarUtil;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.TabLoadObserver;
import org.chromium.chrome.test.util.browser.TabTitleObserver;
import org.chromium.chrome.test.util.browser.webapps.WebappTestPage;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.feature_engagement.CppWrappedTestTracker;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.infobars.InfoBar;
import org.chromium.components.infobars.InfoBarAnimationListener;
import org.chromium.components.infobars.InfoBarUiItem;
import org.chromium.components.messages.DismissReason;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.messages.MessageDispatcherProvider;
import org.chromium.components.messages.MessageIdentifier;
import org.chromium.components.messages.MessagesTestHelper;
import org.chromium.components.site_engagement.SiteEngagementService;
import org.chromium.components.webapps.AppBannerManager;
import org.chromium.components.webapps.AppData;
import org.chromium.components.webapps.AppDetailsDelegate;
import org.chromium.components.webapps.bottomsheet.PwaInstallBottomSheetView;
import org.chromium.components.webapps.installable.InstallableAmbientBadgeInfoBar;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.ModalDialogProperties.ButtonType;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.ButtonCompat;

import java.util.ArrayList;
import java.util.List;

/**
 * Tests the app banners.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class AppBannerManagerTest {
    @Rule
    public ChromeTabbedActivityTestRule mTabbedActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public CustomTabActivityTestRule mCustomTabActivityTestRule = new CustomTabActivityTestRule();

    @Rule
    public ChromeBrowserTestRule mChromeBrowserTestRule = new ChromeBrowserTestRule();

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    // A callback that fires when the IPH system sends an event.
    private final CallbackHelper mOnEventCallback = new CallbackHelper();

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
            mAppData.setPackageInfo(NATIVE_APP_TITLE, mTestServer.getURL(NATIVE_ICON_PATH), 4.5f,
                    NATIVE_APP_INSTALL_TEXT, null, mInstallIntent);
            PostTask.runOrPostTask(
                    TaskTraits.UI_DEFAULT, () -> { mObserver.onAppDetailsRetrieved(mAppData); });
        }

        @Override
        public void destroy() {}
    }

    private static class TestDataStorageFactory extends WebappDataStorage.Factory {
        public String mSplashImage;

        @Override
        public WebappDataStorage create(final String webappId) {
            return new WebappDataStorageWrapper(webappId);
        }

        private class WebappDataStorageWrapper extends WebappDataStorage {
            public WebappDataStorageWrapper(String webappId) {
                super(webappId);
            }

            @Override
            public void updateSplashScreenImage(String splashScreenImage) {
                Assert.assertNull(mSplashImage);
                mSplashImage = splashScreenImage;
            }
        }
    }

    private static class InfobarListener implements InfoBarAnimationListener {
        private boolean mDoneAnimating;

        @Override
        public void notifyAnimationFinished(int animationType) {
            if (animationType == InfoBarAnimationListener.ANIMATION_TYPE_SHOW) {
                mDoneAnimating = true;
            }
        }

        @Override
        public void notifyAllAnimationsFinished(InfoBarUiItem frontInfoBar) {
            mDoneAnimating = true;
        }
    }

    private MockAppDetailsDelegate mDetailsDelegate;
    @Mock
    private PackageManager mPackageManager;
    private EmbeddedTestServer mTestServer;
    private UiDevice mUiDevice;
    private CppWrappedTestTracker mTracker;
    private BottomSheetController mBottomSheetController;

    @Before
    public void setUp() throws Exception {
        AppBannerManager.setIsSupported(true);
        ShortcutHelper.setDelegateForTests(new ShortcutHelper.Delegate() {
            @Override
            public void addShortcutToHomescreen(String id, String title, Bitmap icon,
                    boolean iconAdaptive, Intent shortcutIntent) {
                // Ignore to prevent adding homescreen shortcuts.
            }
        });

        mTracker = new CppWrappedTestTracker(FeatureConstants.PWA_INSTALL_AVAILABLE_FEATURE) {
            @Override
            public void notifyEvent(String event) {
                super.notifyEvent(event);
                mOnEventCallback.notifyCalled();
            }
        };

        ThreadUtils.runOnUiThreadBlocking(() -> {
            Profile profile = Profile.getLastUsedRegularProfile();
            TrackerFactory.setTestingFactory(profile, mTracker);
        });

        mTabbedActivityTestRule.startMainActivityOnBlankPage();
        // Must be set after native has loaded.
        mDetailsDelegate = new MockAppDetailsDelegate();

        ThreadUtils.runOnUiThreadBlocking(
                () -> { AppBannerManager.setAppDetailsDelegate(mDetailsDelegate); });

        AppBannerManager.ignoreChromeChannelForTesting();
        AppBannerManager.setTotalEngagementForTesting(10);
        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        mUiDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());

        mBottomSheetController = mTabbedActivityTestRule.getActivity()
                                         .getRootUiCoordinatorForTesting()
                                         .getBottomSheetController();
    }

    @After
    public void tearDown() {
        if (mTestServer != null) {
            mTestServer.stopAndDestroyServer();
        }
    }

    private void resetEngagementForUrl(final String url, final double engagement) {
        ThreadUtils.runOnUiThreadBlocking(() -> {
            // TODO (https://crbug.com/1063807):  Add incognito mode tests.
            SiteEngagementService.getForBrowserContext(Profile.getLastUsedRegularProfile())
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
        CriteriaHelper.pollUiThread(() -> {
            return getAppBannerManager(tab.getWebContents()).getPipelineStatusForTesting()
                    == expectedValue;
        });
    }

    private void assertAppBannerPipelineStatus(int expectedValue) {
        Tab tab = mTabbedActivityTestRule.getActivity().getActivityTab();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertEquals(expectedValue,
                    getAppBannerManager(tab.getWebContents()).getPipelineStatusForTesting());
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
        CriteriaHelper.pollUiThread(() -> {
            AppBannerManager manager =
                    getAppBannerManager(rule.getActivity().getActivityTab().getWebContents());
            Criteria.checkThat(mDetailsDelegate.mNumRetrieved, Matchers.is(numExpected));
            Criteria.checkThat(manager.isRunningForTesting(), Matchers.is(false));
        });
    }

    private void waitUntilAmbientBadgePromptAppears(
            ChromeActivityTestRule<? extends ChromeActivity> rule) {
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.INSTALLABLE_AMBIENT_BADGE_MESSAGE)) {
            WindowAndroid windowAndroid = rule.getActivity().getWindowAndroid();
            CriteriaHelper.pollUiThread(() -> {
                Criteria.checkThat(
                        MessagesTestHelper.getMessageCount(windowAndroid), Matchers.is(1));
                Criteria.checkThat(MessagesTestHelper.getMessageIdentifier(windowAndroid, 0),
                        Matchers.is(MessageIdentifier.INSTALLABLE_AMBIENT_BADGE));
            });
        } else if (ChromeFeatureList.isEnabled(
                           ChromeFeatureList.INSTALLABLE_AMBIENT_BADGE_INFOBAR)) {
            CriteriaHelper.pollUiThread(() -> {
                List<InfoBar> infobars = rule.getInfoBars();
                Criteria.checkThat(infobars.size(), Matchers.is(1));
                Criteria.checkThat(
                        infobars.get(0), Matchers.instanceOf(InstallableAmbientBadgeInfoBar.class));
            });
        }
    }

    private void checkAmbientBadgePromptNotExist(
            ChromeActivityTestRule<? extends ChromeActivity> rule) {
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.INSTALLABLE_AMBIENT_BADGE_MESSAGE)) {
            WindowAndroid windowAndroid = rule.getActivity().getWindowAndroid();
            TestThreadUtils.runOnUiThreadBlocking(
                    ()
                            -> Assert.assertEquals(
                                    0, MessagesTestHelper.getMessageCount(windowAndroid)));
        } else if (ChromeFeatureList.isEnabled(
                           ChromeFeatureList.INSTALLABLE_AMBIENT_BADGE_INFOBAR)) {
            Assert.assertEquals(0, rule.getInfoBars().size());
        }
    }

    private void waitForBadgeStatus(Tab tab, int expectedValue) {
        CriteriaHelper.pollUiThread(() -> {
            return getAppBannerManager(tab.getWebContents()).getBadgeStatusForTesting()
                    == expectedValue;
        });
    }

    private void waitUntilBottomSheetStatus(ChromeActivityTestRule<? extends ChromeActivity> rule,
            @BottomSheetController.SheetState int status) {
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(mBottomSheetController.getSheetState(), Matchers.is(status));
        });
    }

    private static String getExpectedDialogTitle(Tab tab) throws Exception {
        String title = ThreadUtils.runOnUiThreadBlocking(() -> {
            return TabUtils.getActivity(tab).getString(
                    AppBannerManager.getHomescreenLanguageOption(tab.getWebContents()).titleTextId);
        });
        return title;
    }

    private void waitUntilNoDialogsShowing(final Tab tab) throws Exception {
        UiObject dialogUiObject =
                mUiDevice.findObject(new UiSelector().text(getExpectedDialogTitle(tab)));
        dialogUiObject.waitUntilGone(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL);
    }

    private void tapAndWaitForModalBanner(final Tab tab) throws Exception {
        TouchCommon.singleClickView(tab.getView());

        UiObject dialogUiObject =
                mUiDevice.findObject(new UiSelector().text(getExpectedDialogTitle(tab)));
        Assert.assertTrue(dialogUiObject.waitForExists(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL));
    }

    private void triggerModalWebAppBanner(ChromeActivityTestRule<? extends ChromeActivity> rule,
            String url, boolean installApp) throws Exception {
        resetEngagementForUrl(url, 10);
        rule.loadUrlInNewTab(ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);
        navigateToUrlAndWaitForBannerManager(rule, url);
        waitUntilAmbientBadgePromptAppears(rule);

        Tab tab = rule.getActivity().getActivityTab();
        tapAndWaitForModalBanner(tab);

        if (!installApp) return;

        // Click the button to trigger the adding of the shortcut.
        clickButton(rule.getActivity(), ButtonType.POSITIVE);
    }

    private void triggerModalNativeAppBanner(ChromeActivityTestRule<? extends ChromeActivity> rule,
            String url, String expectedReferrer, boolean installApp) throws Exception {
        resetEngagementForUrl(url, 10);
        rule.loadUrlInNewTab(ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);
        navigateToUrlAndWaitForBannerManager(rule, url);
        waitUntilAppDetailsRetrieved(rule, 1);
        waitUntilAmbientBadgePromptAppears(rule);
        Assert.assertEquals(mDetailsDelegate.mReferrer, expectedReferrer);

        final ChromeActivity activity = rule.getActivity();
        tapAndWaitForModalBanner(activity.getActivityTab());
        if (!installApp) return;

        // Click the button to trigger the installation.
        final ActivityMonitor activityMonitor =
                new ActivityMonitor(new IntentFilter(INSTALL_ACTION),
                        new ActivityResult(Activity.RESULT_OK, null), true);
        Instrumentation instrumentation = InstrumentationRegistry.getInstrumentation();
        instrumentation.addMonitor(activityMonitor);

        ThreadUtils.runOnUiThreadBlocking(() -> {
            String buttonText = activity.getModalDialogManager().getCurrentDialogForTest().get(
                    ModalDialogProperties.POSITIVE_BUTTON_TEXT);
            Assert.assertEquals(NATIVE_APP_INSTALL_TEXT, buttonText);
        });

        clickButton(activity, ButtonType.POSITIVE);

        // Wait until the installation triggers.
        instrumentation.waitForMonitorWithTimeout(
                activityMonitor, CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL);
    }

    private void triggerModalBannerMultipleTimes(
            ChromeActivityTestRule<? extends ChromeActivity> rule, String url,
            boolean isForNativeApp) throws Exception {
        resetEngagementForUrl(url, 10);
        rule.loadUrlInNewTab(ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);
        navigateToUrlAndWaitForBannerManager(rule, url);
        if (isForNativeApp) {
            waitUntilAppDetailsRetrieved(rule, 1);
        }

        waitUntilAmbientBadgePromptAppears(rule);
        Tab tab = rule.getActivity().getActivityTab();
        tapAndWaitForModalBanner(tab);

        // Explicitly dismiss the banner. We should be able to show the banner after dismissing.
        clickButton(rule.getActivity(), ButtonType.NEGATIVE);
        waitUntilNoDialogsShowing(tab);
        tapAndWaitForModalBanner(tab);

        clickButton(rule.getActivity(), ButtonType.NEGATIVE);
        waitUntilNoDialogsShowing(tab);
        tapAndWaitForModalBanner(tab);
    }

    private void triggerBottomSheet(ChromeActivityTestRule<? extends ChromeActivity> rule,
            String url, boolean click) throws Exception {
        resetEngagementForUrl(url, 10);
        rule.loadUrlInNewTab(ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);
        navigateToUrlAndWaitForBannerManager(rule, url);

        if (click) {
            final ChromeActivity activity = rule.getActivity();
            TouchCommon.singleClickView(activity.getActivityTab().getView());
            waitUntilBottomSheetStatus(rule, BottomSheetController.SheetState.FULL);
            return;
        }

        waitUntilBottomSheetStatus(rule, BottomSheetController.SheetState.PEEK);
    }

    private void clickButton(final ChromeActivity activity, @ButtonType final int buttonType) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            PropertyModel model = activity.getModalDialogManager().getCurrentDialogForTest();
            model.get(ModalDialogProperties.CONTROLLER).onClick(model, buttonType);
        });
    }

    private void dismissAmbientBadgeMessage(ChromeActivityTestRule<? extends ChromeActivity> rule)
            throws Exception {
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.INSTALLABLE_AMBIENT_BADGE_MESSAGE)) {
            WindowAndroid windowAndroid = rule.getActivity().getWindowAndroid();

            MessageDispatcher dispatcher = TestThreadUtils.runOnUiThreadBlocking(
                    () -> MessageDispatcherProvider.from(windowAndroid));
            PropertyModel model = TestThreadUtils.runOnUiThreadBlocking(
                    ()
                            -> MessagesTestHelper.getCurrentMessage(
                                    MessagesTestHelper
                                            .getEnqueuedMessages(dispatcher,
                                                    MessageIdentifier.INSTALLABLE_AMBIENT_BADGE)
                                            .get(0)));
            TestThreadUtils.runOnUiThreadBlocking(
                    () -> { dispatcher.dismissMessage(model, DismissReason.GESTURE); });
            CriteriaHelper.pollUiThread(
                    ()
                            -> Criteria.checkThat(MessagesTestHelper.getMessageCount(windowAndroid),
                                    Matchers.is(0)));
        }
    }

    @Test
    @SmallTest
    @Feature({"AppBanners"})
    @CommandLineFlags.Add({"disable-features=" + FeatureConstants.PWA_INSTALL_AVAILABLE_FEATURE})
    public void testAppInstalledEventModalWebAppBannerBrowserTab() throws Exception {
        // Sets the overridden factory to observer splash screen update.
        final TestDataStorageFactory dataStorageFactory = new TestDataStorageFactory();
        WebappDataStorage.setFactoryForTests(dataStorageFactory);

        triggerModalWebAppBanner(mTabbedActivityTestRule,
                WebappTestPage.getServiceWorkerUrlWithAction(
                        mTestServer, "call_stashed_prompt_on_click_verify_appinstalled"),
                true);

        // The appinstalled event should fire (and cause the title to change).
        new TabTitleObserver(mTabbedActivityTestRule.getActivity().getActivityTab(),
                "Got appinstalled: listener, attr")
                .waitForTitleUpdate(3);

        ThreadUtils.runOnUiThread(() -> {
            Assert.assertEquals(1,
                    RecordHistogram.getHistogramValueCountForTesting(
                            "Webapp.Install.InstallEvent", 4 /* API_BROWSER_TAB */));

            Assert.assertEquals(1,
                    RecordHistogram.getHistogramValueCountForTesting(
                            INSTALL_PATH_HISTOGRAM_NAME, /* kApiInitiateInfobar= */ 3));
        });

        // Make sure that the splash screen icon was downloaded.
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(dataStorageFactory.mSplashImage, Matchers.notNullValue());
        });

        // Test that bitmap sizes match expectations.
        int idealSize = mTabbedActivityTestRule.getActivity().getResources().getDimensionPixelSize(
                R.dimen.webapp_splash_image_size_ideal);
        Bitmap splashImage = BitmapHelper.decodeBitmapFromString(dataStorageFactory.mSplashImage);
        Assert.assertEquals(idealSize, splashImage.getWidth());
        Assert.assertEquals(idealSize, splashImage.getHeight());
    }

    @Test
    @SmallTest
    @Feature({"AppBanners"})
    public void testAppInstalledEventModalWebAppBannerCustomTab() throws Exception {
        // Sets the overridden factory to observer splash screen update.
        final TestDataStorageFactory dataStorageFactory = new TestDataStorageFactory();
        WebappDataStorage.setFactoryForTests(dataStorageFactory);

        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(
                CustomTabsIntentTestUtils.createMinimalCustomTabIntent(
                        InstrumentationRegistry.getTargetContext(),
                        ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL));
        triggerModalWebAppBanner(mCustomTabActivityTestRule,
                WebappTestPage.getServiceWorkerUrlWithAction(
                        mTestServer, "call_stashed_prompt_on_click_verify_appinstalled"),
                true);

        // The appinstalled event should fire (and cause the title to change).
        new TabTitleObserver(mCustomTabActivityTestRule.getActivity().getActivityTab(),
                "Got appinstalled: listener, attr")
                .waitForTitleUpdate(3);

        ThreadUtils.runOnUiThread(() -> {
            Assert.assertEquals(1,
                    RecordHistogram.getHistogramValueCountForTesting(
                            "Webapp.Install.InstallEvent", 5 /* API_CUSTOM_TAB */));

            Assert.assertEquals(1,
                    RecordHistogram.getHistogramValueCountForTesting(
                            INSTALL_PATH_HISTOGRAM_NAME, /* kApiInitiatedInfobar= */ 3));
        });

        // Make sure that the splash screen icon was downloaded.
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(dataStorageFactory.mSplashImage, Matchers.notNullValue());
        });

        // Test that bitmap sizes match expectations.
        int idealSize = mTabbedActivityTestRule.getActivity().getResources().getDimensionPixelSize(
                R.dimen.webapp_splash_image_size_ideal);
        Bitmap splashImage = BitmapHelper.decodeBitmapFromString(dataStorageFactory.mSplashImage);
        Assert.assertEquals(idealSize, splashImage.getWidth());
        Assert.assertEquals(idealSize, splashImage.getHeight());
    }

    @Test
    @SmallTest
    @Feature({"AppBanners"})
    public void testAppInstalledModalNativeAppBannerBrowserTab() throws Exception {
        triggerModalNativeAppBanner(mTabbedActivityTestRule,
                WebappTestPage.getNonServiceWorkerUrlWithManifestAndAction(mTestServer,
                        NATIVE_APP_MANIFEST_WITH_ID,
                        "call_stashed_prompt_on_click_verify_appinstalled"),
                NATIVE_APP_BLANK_REFERRER, true);

        // The userChoice promise should resolve (and cause the title to change). appinstalled is
        // not fired for native apps
        new TabTitleObserver(
                mTabbedActivityTestRule.getActivity().getActivityTab(), "Got userChoice: accepted")
                .waitForTitleUpdate(3);

        Assert.assertEquals(
                0, RecordHistogram.getHistogramTotalCountForTesting(INSTALL_PATH_HISTOGRAM_NAME));
    }

    @Test
    @SmallTest
    @Feature({"AppBanners"})
    public void testAppInstalledModalNativeAppBannerBrowserTabWithUrl() throws Exception {
        triggerModalNativeAppBanner(mTabbedActivityTestRule,
                WebappTestPage.getNonServiceWorkerUrlWithManifestAndAction(mTestServer,
                        NATIVE_APP_MANIFEST_WITH_URL,
                        "call_stashed_prompt_on_click_verify_appinstalled"),
                NATIVE_APP_REFERRER, true);

        // The userChoice promise should resolve (and cause the title to change). appinstalled is
        // not fired for native apps
        new TabTitleObserver(
                mTabbedActivityTestRule.getActivity().getActivityTab(), "Got userChoice: accepted")
                .waitForTitleUpdate(3);

        Assert.assertEquals(
                0, RecordHistogram.getHistogramTotalCountForTesting(INSTALL_PATH_HISTOGRAM_NAME));
    }

    @Test
    @SmallTest
    @Feature({"AppBanners"})
    public void testAppInstalledModalNativeAppBannerCustomTab() throws Exception {
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(
                CustomTabsIntentTestUtils.createMinimalCustomTabIntent(
                        InstrumentationRegistry.getTargetContext(),
                        ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL));

        triggerModalNativeAppBanner(mCustomTabActivityTestRule,
                WebappTestPage.getNonServiceWorkerUrlWithManifestAndAction(mTestServer,
                        NATIVE_APP_MANIFEST_WITH_ID,
                        "call_stashed_prompt_on_click_verify_appinstalled"),
                NATIVE_APP_BLANK_REFERRER, true);

        // The appinstalled event should fire (and cause the title to change).
        new TabTitleObserver(mCustomTabActivityTestRule.getActivity().getActivityTab(),
                "Got userChoice: accepted")
                .waitForTitleUpdate(3);

        Assert.assertEquals(
                0, RecordHistogram.getHistogramTotalCountForTesting(INSTALL_PATH_HISTOGRAM_NAME));
    }

    @Test
    @MediumTest
    @Feature({"AppBanners"})
    public void testBlockedModalWebAppBannerResolvesUserChoice() throws Exception {
        triggerModalWebAppBanner(mTabbedActivityTestRule,
                WebappTestPage.getServiceWorkerUrlWithAction(
                        mTestServer, "call_stashed_prompt_on_click"),
                false);

        // Explicitly dismiss the banner.
        final ChromeActivity activity = mTabbedActivityTestRule.getActivity();
        clickButton(activity, ButtonType.NEGATIVE);

        // Ensure userChoice is resolved.
        new TabTitleObserver(activity.getActivityTab(), "Got userChoice: dismissed")
                .waitForTitleUpdate(3);

        Assert.assertEquals(
                0, RecordHistogram.getHistogramTotalCountForTesting(INSTALL_PATH_HISTOGRAM_NAME));
    }

    @Test
    @MediumTest
    @Feature({"AppBanners"})
    @DisabledTest(message = "crbug.com/1144199")
    public void testBlockedModalNativeAppBannerResolveUserChoice() throws Exception {
        triggerModalNativeAppBanner(mTabbedActivityTestRule,
                WebappTestPage.getNonServiceWorkerUrlWithManifestAndAction(
                        mTestServer, NATIVE_APP_MANIFEST_WITH_ID, "call_stashed_prompt_on_click"),
                NATIVE_APP_BLANK_REFERRER, false);

        // Explicitly dismiss the banner.
        final ChromeActivity activity = mTabbedActivityTestRule.getActivity();
        clickButton(activity, ButtonType.NEGATIVE);

        // Ensure userChoice is resolved.
        new TabTitleObserver(activity.getActivityTab(), "Got userChoice: dismissed")
                .waitForTitleUpdate(3);

        Assert.assertEquals(
                0, RecordHistogram.getHistogramTotalCountForTesting(INSTALL_PATH_HISTOGRAM_NAME));
    }

    @Test
    @SmallTest
    @Feature({"AppBanners"})
    public void testModalNativeAppBannerCanBeTriggeredMultipleTimesBrowserTab() throws Exception {
        triggerModalBannerMultipleTimes(mTabbedActivityTestRule,
                WebappTestPage.getNonServiceWorkerUrlWithManifestAndAction(
                        mTestServer, NATIVE_APP_MANIFEST_WITH_ID, "call_stashed_prompt_on_click"),
                true);

        Assert.assertEquals(
                0, RecordHistogram.getHistogramTotalCountForTesting(INSTALL_PATH_HISTOGRAM_NAME));
    }

    @Test
    @SmallTest
    @Feature({"AppBanners"})
    public void testModalNativeAppBannerCanBeTriggeredMultipleTimesCustomTab() throws Exception {
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(
                CustomTabsIntentTestUtils.createMinimalCustomTabIntent(
                        InstrumentationRegistry.getTargetContext(),
                        ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL));

        triggerModalBannerMultipleTimes(mCustomTabActivityTestRule,
                WebappTestPage.getNonServiceWorkerUrlWithManifestAndAction(
                        mTestServer, NATIVE_APP_MANIFEST_WITH_ID, "call_stashed_prompt_on_click"),
                true);

        Assert.assertEquals(
                0, RecordHistogram.getHistogramTotalCountForTesting(INSTALL_PATH_HISTOGRAM_NAME));
    }

    @Test
    @MediumTest
    @Feature({"AppBanners"})
    public void testModalWebAppBannerCanBeTriggeredMultipleTimesBrowserTab() throws Exception {
        triggerModalBannerMultipleTimes(mTabbedActivityTestRule,
                WebappTestPage.getServiceWorkerUrlWithAction(
                        mTestServer, "call_stashed_prompt_on_click"),
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
                        InstrumentationRegistry.getTargetContext(),
                        ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL));

        triggerModalBannerMultipleTimes(mCustomTabActivityTestRule,
                WebappTestPage.getServiceWorkerUrlWithAction(
                        mTestServer, "call_stashed_prompt_on_click"),
                false);

        Assert.assertEquals(
                0, RecordHistogram.getHistogramTotalCountForTesting(INSTALL_PATH_HISTOGRAM_NAME));
    }

    @Test
    @MediumTest
    @Feature({"AppBanners"})
    @CommandLineFlags.Add({"enable-features=" + ChromeFeatureList.INSTALLABLE_AMBIENT_BADGE_INFOBAR,
            "disable-features=" + ChromeFeatureList.INSTALLABLE_AMBIENT_BADGE_MESSAGE})
    public void
    testBlockedAmbientBadgeDoesNotAppearAgainForMonths() throws Exception {
        // Visit a site that is a PWA. The ambient badge should show.
        String webBannerUrl = WebappTestPage.getServiceWorkerUrl(mTestServer);
        resetEngagementForUrl(webBannerUrl, 10);

        InfoBarContainer container = mTabbedActivityTestRule.getInfoBarContainer();
        final InfobarListener listener = new InfobarListener();
        TestThreadUtils.runOnUiThreadBlocking(() -> container.addAnimationListener(listener));

        Tab tab = mTabbedActivityTestRule.getActivity().getActivityTab();
        new TabLoadObserver(tab).fullyLoadUrl(webBannerUrl);
        waitUntilAmbientBadgePromptAppears(mTabbedActivityTestRule);

        // Explicitly dismiss the ambient badge.
        CriteriaHelper.pollUiThread(() -> listener.mDoneAnimating);

        ArrayList<InfoBar> infobars = container.getInfoBarsForTesting();
        View close = infobars.get(0).getView().findViewById(R.id.infobar_close_button);
        TouchCommon.singleClickView(close);
        InfoBarUtil.waitUntilNoInfoBarsExist(mTabbedActivityTestRule.getInfoBars());

        // Waiting two months shouldn't be long enough.
        AppBannerManager.setTimeDeltaForTesting(61);
        new TabLoadObserver(tab).fullyLoadUrl(webBannerUrl);
        waitForBadgeStatus(tab, AmbientBadgeState.BLOCKED);
        checkAmbientBadgePromptNotExist(mTabbedActivityTestRule);

        AppBannerManager.setTimeDeltaForTesting(62);
        new TabLoadObserver(tab).fullyLoadUrl(webBannerUrl);
        waitForBadgeStatus(tab, AmbientBadgeState.BLOCKED);
        checkAmbientBadgePromptNotExist(mTabbedActivityTestRule);

        // Waiting three months should allow the ambient badge to reappear.
        AppBannerManager.setTimeDeltaForTesting(91);
        new TabLoadObserver(tab).fullyLoadUrl(webBannerUrl);
        waitForBadgeStatus(tab, AmbientBadgeState.SHOWING);
        waitUntilAmbientBadgePromptAppears(mTabbedActivityTestRule);

        Assert.assertEquals(
                0, RecordHistogram.getHistogramTotalCountForTesting(INSTALL_PATH_HISTOGRAM_NAME));
    }

    @Test
    @MediumTest
    @Feature({"AppBanners"})
    @CommandLineFlags.
    Add({"enable-features=" + ChromeFeatureList.INSTALLABLE_AMBIENT_BADGE_MESSAGE + "<Study",
            "force-fieldtrials=Study/Group",
            "force-fieldtrial-params="
                    + "Study.Group:installable_ambient_badge_message_throttle_domains_capacity/0",
            "disable-features=" + ChromeFeatureList.INSTALLABLE_AMBIENT_BADGE_INFOBAR})
    public void
    testBlockedAmbientBadgeDoesNotAppearAgainForMonths_Message() throws Exception {
        // Visit a site that is a PWA. The ambient badge should show.
        String webBannerUrl = WebappTestPage.getServiceWorkerUrl(mTestServer);
        resetEngagementForUrl(webBannerUrl, 10);
        Tab tab = mTabbedActivityTestRule.getActivity().getActivityTab();
        new TabLoadObserver(tab).fullyLoadUrl(webBannerUrl);
        waitUntilAmbientBadgePromptAppears(mTabbedActivityTestRule);

        WindowAndroid windowAndroid = mTabbedActivityTestRule.getActivity().getWindowAndroid();

        // Explicitly dismiss the ambient badge.
        CriteriaHelper.pollUiThread(
                ()
                        -> Criteria.checkThat(
                                MessagesTestHelper.getMessageCount(windowAndroid), Matchers.is(1)));

        MessageDispatcher dispatcher = TestThreadUtils.runOnUiThreadBlocking(
                () -> MessageDispatcherProvider.from(windowAndroid));
        PropertyModel model = TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> MessagesTestHelper.getCurrentMessage(
                                MessagesTestHelper
                                        .getEnqueuedMessages(dispatcher,
                                                MessageIdentifier.INSTALLABLE_AMBIENT_BADGE)
                                        .get(0)));
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { dispatcher.dismissMessage(model, DismissReason.GESTURE); });
        CriteriaHelper.pollUiThread(
                ()
                        -> Criteria.checkThat(
                                MessagesTestHelper.getMessageCount(windowAndroid), Matchers.is(0)));

        // Waiting two months shouldn't be long enough.
        AppBannerManager.setTimeDeltaForTesting(61);
        new TabLoadObserver(tab).fullyLoadUrl(webBannerUrl);
        CriteriaHelper.pollUiThread(
                ()
                        -> Criteria.checkThat(
                                MessagesTestHelper.getMessageCount(windowAndroid), Matchers.is(0)));

        AppBannerManager.setTimeDeltaForTesting(62);
        new TabLoadObserver(tab).fullyLoadUrl(webBannerUrl);
        CriteriaHelper.pollUiThread(
                ()
                        -> Criteria.checkThat(
                                MessagesTestHelper.getMessageCount(windowAndroid), Matchers.is(0)));

        // Waiting three months should allow the ambient badge to reappear.
        AppBannerManager.setTimeDeltaForTesting(91);
        new TabLoadObserver(tab).fullyLoadUrl(webBannerUrl);
        CriteriaHelper.pollUiThread(
                ()
                        -> Criteria.checkThat(
                                MessagesTestHelper.getMessageCount(windowAndroid), Matchers.is(1)));

        Assert.assertEquals(
                0, RecordHistogram.getHistogramTotalCountForTesting(INSTALL_PATH_HISTOGRAM_NAME));
    }

    @Test
    @MediumTest
    @Feature({"AppBanners"})
    @CommandLineFlags.Add("enable-features=" + ChromeFeatureList.INSTALLABLE_AMBIENT_BADGE_INFOBAR)
    public void testAmbientBadgeDoesNotAppearWhenEventCanceled() throws Exception {
        String webBannerUrl = WebappTestPage.getServiceWorkerUrlWithAction(
                mTestServer, "stash_event_and_prevent_default");
        resetEngagementForUrl(webBannerUrl, 10);
        navigateToUrlAndWaitForBannerManager(mTabbedActivityTestRule, webBannerUrl);

        assertAppBannerPipelineStatus(AppBannerManagerState.PENDING_PROMPT_CANCELED);
        // As the page called preventDefault on the beforeinstallprompt event, we do not expect to
        // see an ambient badge.
        checkAmbientBadgePromptNotExist(mTabbedActivityTestRule);

        // Even after waiting for three months, there should not be no ambient badge.
        AppBannerManager.setTimeDeltaForTesting(91);
        navigateToUrlAndWaitForBannerManager(mTabbedActivityTestRule, webBannerUrl);

        assertAppBannerPipelineStatus(AppBannerManagerState.PENDING_PROMPT_CANCELED);
        checkAmbientBadgePromptNotExist(mTabbedActivityTestRule);

        // When the page is ready and calls prompt() on the beforeinstallprompt event, only then we
        // expect to see the modal banner.
        Tab tab = mTabbedActivityTestRule.getActivity().getActivityTab();
        tapAndWaitForModalBanner(tab);
    }

    @Test
    @SmallTest
    @Feature({"AppBanners"})
    public void testModalWebAppBannerTriggeredWithUnsupportedNativeApp() throws Exception {
        // The web app banner should show if preferred_related_applications is true but there is no
        // supported application platform specified in the related applications list.
        triggerModalWebAppBanner(mTabbedActivityTestRule,
                WebappTestPage.getServiceWorkerUrlWithManifestAndAction(mTestServer,
                        WEB_APP_MANIFEST_WITH_UNSUPPORTED_PLATFORM, "call_stashed_prompt_on_click"),
                false);

        Assert.assertEquals(
                0, RecordHistogram.getHistogramTotalCountForTesting(INSTALL_PATH_HISTOGRAM_NAME));
    }

    @Test
    @SmallTest
    @Feature({"AppBanners"})
    public void testBottomSheet() throws Exception {
        triggerBottomSheet(mTabbedActivityTestRule,
                WebappTestPage.getServiceWorkerUrlWithManifest(
                        mTestServer, WEB_APP_MANIFEST_FOR_BOTTOM_SHEET_INSTALL),
                /*click=*/false);

        View toolbar = mBottomSheetController.getCurrentSheetContent().getToolbarView();
        View content = mBottomSheetController.getCurrentSheetContent().getContentView();

        // Expand the bottom sheet via drag handle.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ImageView dragHandle = toolbar.findViewById(R.id.drag_handlebar);
            TouchCommon.singleClickView(dragHandle);
        });

        waitUntilBottomSheetStatus(mTabbedActivityTestRule, BottomSheetController.SheetState.FULL);

        TextView appName =
                toolbar.findViewById(PwaInstallBottomSheetView.getAppNameViewIdForTesting());
        TextView appOrigin =
                toolbar.findViewById(PwaInstallBottomSheetView.getAppOriginViewIdForTesting());
        TextView description =
                content.findViewById(PwaInstallBottomSheetView.getDescViewIdForTesting());

        Assert.assertEquals("PWA Bottom Sheet", appName.getText());
        Assert.assertTrue(appOrigin.getText().toString().startsWith("http://127.0.0.1:"));
        Assert.assertEquals("Lorem ipsum dolor sit amet, consectetur adipiscing elit, "
                        + "sed do eiusmod tempor incididunt ut labore et dolore magna aliqua",
                description.getText());

        // Collapse the bottom sheet.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ImageView dragHandle = toolbar.findViewById(R.id.drag_handlebar);
            TouchCommon.singleClickView(dragHandle);
        });

        waitUntilBottomSheetStatus(mTabbedActivityTestRule, BottomSheetController.SheetState.PEEK);

        // Dismiss the bottom sheet.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mBottomSheetController.hideContent(
                    mBottomSheetController.getCurrentSheetContent(), false);
        });

        waitUntilBottomSheetStatus(
                mTabbedActivityTestRule, BottomSheetController.SheetState.HIDDEN);

        Assert.assertEquals(
                0, RecordHistogram.getHistogramTotalCountForTesting(INSTALL_PATH_HISTOGRAM_NAME));
    }

    @Test
    @MediumTest
    @Feature({"AppBanners"})
    @CommandLineFlags.Add("disable-features=" + FeatureConstants.PWA_INSTALL_AVAILABLE_FEATURE)
    public void testAppInstalledEventBottomSheet() throws Exception {
        triggerBottomSheet(mTabbedActivityTestRule,
                WebappTestPage.getServiceWorkerUrlWithManifestAndAction(mTestServer,
                        WEB_APP_MANIFEST_FOR_BOTTOM_SHEET_INSTALL,
                        "call_stashed_prompt_on_click_verify_appinstalled"),
                /*click=*/true);

        View toolbar = mBottomSheetController.getCurrentSheetContent().getToolbarView();

        // Install app from the bottom sheet.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ButtonCompat buttonInstall = toolbar.findViewById(
                    PwaInstallBottomSheetView.getButtonInstallViewIdForTesting());
            TouchCommon.singleClickView(buttonInstall);
        });

        waitUntilBottomSheetStatus(
                mTabbedActivityTestRule, BottomSheetController.SheetState.HIDDEN);

        // The appinstalled event should fire (and cause the title to change).
        new TabTitleObserver(mTabbedActivityTestRule.getActivity().getActivityTab(),
                "Got appinstalled: listener, attr")
                .waitForTitleUpdate(3);

        ThreadUtils.runOnUiThread(() -> {
            Assert.assertEquals(1,
                    RecordHistogram.getHistogramValueCountForTesting(
                            "Webapp.Install.InstallEvent", 4 /* API_BROWSER_TAB */));

            Assert.assertEquals(1,
                    RecordHistogram.getHistogramValueCountForTesting(
                            INSTALL_PATH_HISTOGRAM_NAME, /* kApiInitiateBottomSheet= */ 6));
        });
    }

    @Test
    @MediumTest
    @Feature({"AppBanners"})
    public void testDismissBottomSheetResolvesUserChoice() throws Exception {
        triggerBottomSheet(mTabbedActivityTestRule,
                WebappTestPage.getServiceWorkerUrlWithManifestAndAction(mTestServer,
                        WEB_APP_MANIFEST_FOR_BOTTOM_SHEET_INSTALL, "call_stashed_prompt_on_click"),
                /*click=*/true);

        // Dismiss the bottom sheet.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mBottomSheetController.hideContent(
                    mBottomSheetController.getCurrentSheetContent(), false);
        });

        waitUntilBottomSheetStatus(
                mTabbedActivityTestRule, BottomSheetController.SheetState.HIDDEN);

        // Ensure userChoice is resolved.
        new TabTitleObserver(
                mTabbedActivityTestRule.getActivity().getActivityTab(), "Got userChoice: dismissed")
                .waitForTitleUpdate(3);

        Assert.assertEquals(
                0, RecordHistogram.getHistogramTotalCountForTesting(INSTALL_PATH_HISTOGRAM_NAME));
    }

    @Test
    @MediumTest
    @Feature({"AppBanners"})
    public void testBlockedBottomSheetDoesNotAppearAgainForMonths() throws Exception {
        String url = WebappTestPage.getServiceWorkerUrlWithManifestAndAction(mTestServer,
                WEB_APP_MANIFEST_FOR_BOTTOM_SHEET_INSTALL, "call_stashed_prompt_on_click");
        triggerBottomSheet(mTabbedActivityTestRule, url, /*click=*/true);

        // Dismiss the bottom sheet after expanding it.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mBottomSheetController.hideContent(
                    mBottomSheetController.getCurrentSheetContent(), false);
        });
        waitUntilBottomSheetStatus(
                mTabbedActivityTestRule, BottomSheetController.SheetState.HIDDEN);

        // Waiting two months shouldn't be long enough.
        AppBannerManager.setTimeDeltaForTesting(61);
        navigateToUrlAndWaitForBannerManager(mTabbedActivityTestRule, url);
        waitUntilBottomSheetStatus(
                mTabbedActivityTestRule, BottomSheetController.SheetState.HIDDEN);

        AppBannerManager.setTimeDeltaForTesting(62);
        navigateToUrlAndWaitForBannerManager(mTabbedActivityTestRule, url);
        waitUntilBottomSheetStatus(
                mTabbedActivityTestRule, BottomSheetController.SheetState.HIDDEN);

        // Waiting three months should allow the bottom sheet to reappear.
        AppBannerManager.setTimeDeltaForTesting(91);
        navigateToUrlAndWaitForBannerManager(mTabbedActivityTestRule, url);
        waitUntilBottomSheetStatus(mTabbedActivityTestRule, BottomSheetController.SheetState.PEEK);
    }

    @Test
    @MediumTest
    @Feature({"AppBanners"})
    public void testBottomSheetSkipsHiddenWebContents() throws Exception {
        String url = WebappTestPage.getServiceWorkerUrlWithManifestAndAction(mTestServer,
                WEB_APP_MANIFEST_FOR_BOTTOM_SHEET_INSTALL, "call_stashed_prompt_on_click");

        resetEngagementForUrl(url, 10);
        mTabbedActivityTestRule.loadUrl(ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);

        // Create an extra tab so that there is a background tab.
        ChromeTabUtils.newTabFromMenu(InstrumentationRegistry.getInstrumentation(),
                mTabbedActivityTestRule.getActivity(),
                /* isIncognito= */ false, /* waitForNtpLoad= */ true);

        Tab backgroundTab = mTabbedActivityTestRule.getActivity().getCurrentTabModel().getTabAt(0);
        Assert.assertTrue(backgroundTab != null);

        TestThreadUtils.runOnUiThreadBlocking(
                () -> { backgroundTab.loadUrl(new LoadUrlParams(url)); });

        waitForAppBannerPipelineStatus(
                backgroundTab, AppBannerManagerState.PENDING_PROMPT_NOT_CANCELED);

        ThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertEquals(BottomSheetController.SheetState.HIDDEN,
                    mBottomSheetController.getSheetState());
        });
    }

    @Test
    @MediumTest
    @Feature({"AppBanners"})
    @CommandLineFlags.Add({"enable-features=" + FeatureConstants.PWA_INSTALL_AVAILABLE_FEATURE + ","
                    + ChromeFeatureList.INSTALLABLE_AMBIENT_BADGE_INFOBAR,
            "disable-features=" + ChromeFeatureList.ADD_TO_HOMESCREEN_IPH + ","
                    + ChromeFeatureList.INSTALLABLE_AMBIENT_BADGE_MESSAGE})
    public void
    testInProductHelp() throws Exception {
        // Visit a site that is a PWA. The ambient badge should show.
        String webBannerUrl = WebappTestPage.getServiceWorkerUrl(mTestServer);
        resetEngagementForUrl(webBannerUrl, 10);

        InfoBarContainer container = mTabbedActivityTestRule.getInfoBarContainer();
        final InfobarListener listener = new InfobarListener();
        TestThreadUtils.runOnUiThreadBlocking(() -> container.addAnimationListener(listener));

        Tab tab = mTabbedActivityTestRule.getActivity().getActivityTab();
        new TabLoadObserver(tab).fullyLoadUrl(webBannerUrl);
        waitUntilAmbientBadgePromptAppears(mTabbedActivityTestRule);

        waitForHelpBubble(withText(R.string.iph_pwa_install_available_text)).perform(click());
        assertThat(mTracker.wasDismissed(), is(true));

        int callCount = mOnEventCallback.getCallCount();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            AppMenuCoordinator coordinator = mTabbedActivityTestRule.getAppMenuCoordinator();
            AppMenuTestSupport.showAppMenu(coordinator, null, false);
            AppMenuTestSupport.callOnItemClick(coordinator, R.id.install_webapp_id);
        });
        mOnEventCallback.waitForCallback(callCount, 1);

        assertThat(mTracker.getLastEvent(), is(EventConstants.PWA_INSTALL_MENU_SELECTED));

        Assert.assertEquals(
                0, RecordHistogram.getHistogramTotalCountForTesting(INSTALL_PATH_HISTOGRAM_NAME));
    }

    @Test
    @MediumTest
    @Feature({"AppBanners"})
    @CommandLineFlags.Add({"enable-features=" + FeatureConstants.PWA_INSTALL_AVAILABLE_FEATURE + ","
                    + ChromeFeatureList.INSTALLABLE_AMBIENT_BADGE_MESSAGE,
            "disable-features=" + ChromeFeatureList.ADD_TO_HOMESCREEN_IPH + ","
                    + ChromeFeatureList.INSTALLABLE_AMBIENT_BADGE_INFOBAR})
    public void
    testInProductHelp_Message() throws Exception {
        // Visit a site that is a PWA. The ambient badge should show.
        String webBannerUrl = WebappTestPage.getServiceWorkerUrl(mTestServer);
        resetEngagementForUrl(webBannerUrl, 10);

        Tab tab = mTabbedActivityTestRule.getActivity().getActivityTab();
        new TabLoadObserver(tab).fullyLoadUrl(webBannerUrl);
        waitUntilAmbientBadgePromptAppears(mTabbedActivityTestRule);
        // Dismiss the message so it will not overlap with the IPH.
        dismissAmbientBadgeMessage(mTabbedActivityTestRule);

        waitForHelpBubble(withText(R.string.iph_pwa_install_available_text)).perform(click());
        assertThat(mTracker.wasDismissed(), is(true));

        int callCount = mOnEventCallback.getCallCount();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            AppMenuCoordinator coordinator = mTabbedActivityTestRule.getAppMenuCoordinator();
            AppMenuTestSupport.showAppMenu(coordinator, null, false);
            AppMenuTestSupport.callOnItemClick(coordinator, R.id.install_webapp_id);
        });
        mOnEventCallback.waitForCallback(callCount, 1);

        assertThat(mTracker.getLastEvent(), is(EventConstants.PWA_INSTALL_MENU_SELECTED));

        Assert.assertEquals(
                0, RecordHistogram.getHistogramTotalCountForTesting(INSTALL_PATH_HISTOGRAM_NAME));
    }

    private ViewInteraction waitForHelpBubble(Matcher<View> matcher) {
        View mainDecorView = mTabbedActivityTestRule.getActivity().getWindow().getDecorView();
        return onView(isRoot())
                .inRoot(RootMatchers.withDecorView(not(is(mainDecorView))))
                .check(waitForView(matcher));
    }

    private void assertNoHelpBubble(Matcher<View> matcher) {
        View mainDecorView = mTabbedActivityTestRule.getActivity().getWindow().getDecorView();
        onView(isRoot())
                .inRoot(RootMatchers.withDecorView(isDisplayed()))
                .check(waitForView(matcher, VIEW_NULL));
    }

    @Test
    @MediumTest
    @Feature({"AppBanners"})
    public void testInProductHelpSkipsHiddenWebContents() throws Exception {
        String url = WebappTestPage.getServiceWorkerUrlWithManifestAndAction(mTestServer,
                WEB_APP_MANIFEST_FOR_BOTTOM_SHEET_INSTALL, "call_stashed_prompt_on_click");

        resetEngagementForUrl(url, 10);
        mTabbedActivityTestRule.loadUrl(ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);

        // Create an extra tab so that there is a background tab.
        ChromeTabUtils.newTabFromMenu(InstrumentationRegistry.getInstrumentation(),
                mTabbedActivityTestRule.getActivity(),
                /* isIncognito= */ false, /* waitForNtpLoad= */ true);

        Tab backgroundTab = mTabbedActivityTestRule.getActivity().getCurrentTabModel().getTabAt(0);
        Assert.assertTrue(backgroundTab != null);

        TestThreadUtils.runOnUiThreadBlocking(
                () -> { backgroundTab.loadUrl(new LoadUrlParams(url)); });

        waitForAppBannerPipelineStatus(
                backgroundTab, AppBannerManagerState.PENDING_PROMPT_NOT_CANCELED);

        assertNoHelpBubble(withText(R.string.iph_pwa_install_available_text));
    }

    @Test
    @MediumTest
    @Feature({"AppBanners"})
    @Features.EnableFeatures({ChromeFeatureList.INSTALLABLE_AMBIENT_BADGE_INFOBAR,
            ChromeFeatureList.SKIP_SERVICE_WORKER_FOR_INSTALL_PROMPT})
    public void
    testAmbientBadgeDoesNotAppearWhenNoServiceWorker() throws Exception {
        String webBannerUrl = WebappTestPage.getNonServiceWorkerUrlWithAction(
                mTestServer, "call_stashed_prompt_on_click");
        resetEngagementForUrl(webBannerUrl, 10);
        navigateToUrlAndWaitForBannerManager(mTabbedActivityTestRule, webBannerUrl);

        // As the page doesn't have service worker, we do not expect to
        // see an ambient badge.
        Tab tab = mTabbedActivityTestRule.getActivity().getActivityTab();
        waitForBadgeStatus(tab, AmbientBadgeState.PENDING_WORKER);
        checkAmbientBadgePromptNotExist(mTabbedActivityTestRule);

        // Tap to trigger beforeinstallprompt.prompt, we expect to see the modal banner.
        tapAndWaitForModalBanner(tab);
    }

    @Test
    @MediumTest
    @Feature({"AppBanners"})
    @Features.EnableFeatures({ChromeFeatureList.SKIP_SERVICE_WORKER_FOR_INSTALL_PROMPT})
    public void testAmbientBadgeAppearWithServiceWorkerPage() throws Exception {
        String webBannerUrl = WebappTestPage.getServiceWorkerUrlWithAction(
                mTestServer, "call_stashed_prompt_on_click");
        resetEngagementForUrl(webBannerUrl, 10);
        navigateToUrlAndWaitForBannerManager(mTabbedActivityTestRule, webBannerUrl);

        Tab tab = mTabbedActivityTestRule.getActivity().getActivityTab();
        waitForBadgeStatus(tab, AmbientBadgeState.SHOWING);
        waitUntilAmbientBadgePromptAppears(mTabbedActivityTestRule);

        // Calls prompt() on the beforeinstallprompt event, we expect to see the modal banner.
        tapAndWaitForModalBanner(tab);
    }

    @Test
    @MediumTest
    @Feature({"AppBanners"})
    public void testAppBannerDismissedAfterNavigation() throws Exception {
        String url = WebappTestPage.getServiceWorkerUrlWithAction(
                mTestServer, "call_stashed_prompt_on_click");
        resetEngagementForUrl(url, 10);

        mTabbedActivityTestRule.loadUrlInNewTab(ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);
        navigateToUrlAndWaitForBannerManager(mTabbedActivityTestRule, url);
        waitUntilAmbientBadgePromptAppears(mTabbedActivityTestRule);

        Tab tab = mTabbedActivityTestRule.getActivity().getActivityTab();
        tapAndWaitForModalBanner(tab);

        // Navigate and check that the dialog was dismissed.
        mTabbedActivityTestRule.loadUrl(ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);
        waitUntilNoDialogsShowing(tab);
    }

    @Test
    @SmallTest
    @Feature({"AppBanners"})
    public void testAmbientBadgeTriggeredWithListedRelatedApp() throws Exception {
        // The ambient badge should show if there is play app in related applications list but
        // preferred_related_applications is false.
        String webBannerUrl = WebappTestPage.getServiceWorkerUrlWithAction(
                mTestServer, "call_stashed_prompt_on_click");
        resetEngagementForUrl(webBannerUrl, 10);
        navigateToUrlAndWaitForBannerManager(mTabbedActivityTestRule, webBannerUrl);

        waitUntilAmbientBadgePromptAppears(mTabbedActivityTestRule);

        // Calls prompt() on the beforeinstallprompt event, we expect to see the modal banner.
        Tab tab = mTabbedActivityTestRule.getActivity().getActivityTab();
        tapAndWaitForModalBanner(tab);
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
    @Feature({"AppBanners"})
    public void testAmbientBadgeDoesNotAppearWhenRelatedAppInstalled() throws Exception {
        String url = WebappTestPage.getServiceWorkerUrlWithManifestAndAction(mTestServer,
                WEB_APP_MANIFEST_WITH_RELATED_APP_LIST, "call_stashed_prompt_on_click");
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
    @Feature({"AppBanners"})
    @CommandLineFlags.Add({"enable-features=AmbientBadgeSiteEngagement:minimal_engagement/100"})
    public void testAmbientBadgeInsufficientEngagement() throws Exception {
        String url = WebappTestPage.getServiceWorkerUrlWithAction(
                mTestServer, "call_stashed_prompt_on_click");
        // Set the engagement to trigger beforeinstall event but not ambient badge.
        resetEngagementForUrl(url, 10);

        navigateToUrlAndWaitForBannerManager(mTabbedActivityTestRule, url);

        assertAppBannerPipelineStatus(AppBannerManagerState.PENDING_PROMPT_NOT_CANCELED);

        Tab tab = mTabbedActivityTestRule.getActivity().getActivityTab();
        waitForBadgeStatus(tab, AmbientBadgeState.PENDING_ENGAGEMENT);
        checkAmbientBadgePromptNotExist(mTabbedActivityTestRule);

        // Calls prompt() on the beforeinstallprompt event, we expect to see the modal banner.
        tapAndWaitForModalBanner(tab);
    }

    @Test
    @SmallTest
    @Feature({"AppBanners"})
    @CommandLineFlags.Add({"enable-features=AmbientBadgeSiteEngagement:minimal_engagement/100"})
    public void testAmbientBadgeSufficientEngagement() throws Exception {
        String url = WebappTestPage.getServiceWorkerUrlWithAction(
                mTestServer, "call_stashed_prompt_on_click");
        // Set the engagement big enough for ambient badge.
        resetEngagementForUrl(url, 100);

        navigateToUrlAndWaitForBannerManager(mTabbedActivityTestRule, url);

        assertAppBannerPipelineStatus(AppBannerManagerState.PENDING_PROMPT_NOT_CANCELED);

        Tab tab = mTabbedActivityTestRule.getActivity().getActivityTab();
        waitForBadgeStatus(tab, AmbientBadgeState.SHOWING);
        waitUntilAmbientBadgePromptAppears(mTabbedActivityTestRule);

        // Calls prompt() on the beforeinstallprompt event, we expect to see the modal banner.
        tapAndWaitForModalBanner(tab);
    }

    @Test
    @SmallTest
    @Feature({"AppBanners"})
    @CommandLineFlags.Add({"enable-features=AmbientBadgeSuppressFirstVisit:period/7d"})
    public void testAmbientBadgeSuppressedOnFirstVisit() throws Exception {
        String url = WebappTestPage.getServiceWorkerUrlWithAction(
                mTestServer, "call_stashed_prompt_on_click");
        resetEngagementForUrl(url, 10);

        navigateToUrlAndWaitForBannerManager(mTabbedActivityTestRule, url);

        assertAppBannerPipelineStatus(AppBannerManagerState.PENDING_PROMPT_NOT_CANCELED);

        Tab tab = mTabbedActivityTestRule.getActivity().getActivityTab();
        waitForBadgeStatus(tab, AmbientBadgeState.PENDING_ENGAGEMENT);

        // Advance 3days and navigate to |url| again, ambient badge should show.
        AppBannerManager.setTimeDeltaForTesting(3);
        mTabbedActivityTestRule.loadUrl(ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);
        navigateToUrlAndWaitForBannerManager(mTabbedActivityTestRule, url);
        waitForBadgeStatus(tab, AmbientBadgeState.SHOWING);
        waitUntilAmbientBadgePromptAppears(mTabbedActivityTestRule);

        // Advance 8 more days and navigate to |url| again, no ambient badge.
        AppBannerManager.setTimeDeltaForTesting(11);
        mTabbedActivityTestRule.loadUrl(ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);
        navigateToUrlAndWaitForBannerManager(mTabbedActivityTestRule, url);
        waitForBadgeStatus(tab, AmbientBadgeState.PENDING_ENGAGEMENT);
        checkAmbientBadgePromptNotExist(mTabbedActivityTestRule);

        // Calls prompt() on the beforeinstallprompt event and wait for the banner.
        tapAndWaitForModalBanner(tab);
    }
}
