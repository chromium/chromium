// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.banners;

import android.app.Activity;
import android.app.Instrumentation;
import android.app.Instrumentation.ActivityMonitor;
import android.app.Instrumentation.ActivityResult;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.graphics.Bitmap;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;
import android.support.test.filters.SmallTest;
import android.support.v7.app.AlertDialog;
import android.text.TextUtils;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentMatchers;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.customtabs.CustomTabsTestUtils;
import org.chromium.chrome.browser.engagement.SiteEngagementService;
import org.chromium.chrome.browser.infobar.AppBannerInfoBarAndroid;
import org.chromium.chrome.browser.infobar.InfoBar;
import org.chromium.chrome.browser.infobar.InfoBarContainer;
import org.chromium.chrome.browser.infobar.InfoBarContainer.InfoBarAnimationListener;
import org.chromium.chrome.browser.infobar.InfoBarContainerLayout.Item;
import org.chromium.chrome.browser.infobar.InstallableAmbientBadgeInfoBar;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.webapps.AddToHomescreenDialog;
import org.chromium.chrome.browser.webapps.WebappDataStorage;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.InfoBarUtil;
import org.chromium.chrome.test.util.browser.TabLoadObserver;
import org.chromium.chrome.test.util.browser.TabTitleObserver;
import org.chromium.chrome.test.util.browser.WebappTestPage;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.Callable;

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
    public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    private static final String NATIVE_APP_MANIFEST_WITH_ID =
            "/chrome/test/data/banners/play_app_manifest.json";

    private static final String NATIVE_APP_MANIFEST_WITH_URL =
            "/chrome/test/data/banners/play_app_url_manifest.json";

    private static final String NATIVE_ICON_PATH = "/chrome/test/data/banners/launcher-icon-4x.png";

    private static final String NATIVE_APP_TITLE = "Mock app title";

    private static final String NATIVE_APP_PACKAGE = "123456";

    private static final String NATIVE_APP_INSTALL_TEXT = "Install this";

    private static final String NATIVE_APP_REFERRER = "chrome_inline&playinline=chrome_inline";

    private static final String NATIVE_APP_BLANK_REFERRER = "playinline=chrome_inline";

    private static final String WEB_APP_SHORT_TITLE_MANIFEST =
            "/chrome/test/data/banners/manifest_short_name_only.json";

    private static final String WEB_APP_EMPTY_NAME_MANIFEST =
            "/chrome/test/data/banners/manifest_empty_name.json";

    private static final String WEB_APP_TITLE = "Manifest test app";

    private static final String WEB_APP_SHORT_TITLE = "Manifest";

    private static final String INSTALL_ACTION = "INSTALL_ACTION";

    private class MockAppDetailsDelegate extends AppDetailsDelegate {
        private Observer mObserver;
        private AppData mAppData;
        private int mNumRetrieved;
        private Intent mInstallIntent;
        private String mReferrer;

        @Override
        protected void getAppDetailsAsynchronously(
                Observer observer, String url, String packageName, String referrer, int iconSize) {
            mNumRetrieved += 1;
            mObserver = observer;
            mReferrer = referrer;
            mInstallIntent = new Intent(INSTALL_ACTION);

            mAppData = new AppData(url, packageName);
            mAppData.setPackageInfo(NATIVE_APP_TITLE, mTestServer.getURL(NATIVE_ICON_PATH), 4.5f,
                    NATIVE_APP_INSTALL_TEXT, null, mInstallIntent);
            ThreadUtils.runOnUiThread(() -> { mObserver.onAppDetailsRetrieved(mAppData); });
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
        public void notifyAllAnimationsFinished(Item frontInfoBar) {}
    }

    private MockAppDetailsDelegate mDetailsDelegate;
    @Mock
    private PackageManager mPackageManager;
    private EmbeddedTestServer mTestServer;

    @Before
    public void setUp() throws Exception {
        AppBannerManager.setIsSupported(true);
        InstallerDelegate.setPackageManagerForTesting(mPackageManager);
        ShortcutHelper.setDelegateForTests(new ShortcutHelper.Delegate() {
            @Override
            public void addShortcutToHomescreen(String title, Bitmap icon, Intent shortcutIntent) {
                // Ignore to prevent adding homescreen shortcuts.
            }
        });

        mTabbedActivityTestRule.startMainActivityOnBlankPage();
        // Must be set after native has loaded.
        mDetailsDelegate = new MockAppDetailsDelegate();
        ThreadUtils.runOnUiThreadBlocking(
                () -> { AppBannerManager.setAppDetailsDelegate(mDetailsDelegate); });

        AppBannerManager.setTotalEngagementForTesting(10);
        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
    }

    @After
    public void tearDown() throws Exception {
        mTestServer.stopAndDestroyServer();
    }

    private void resetEngagementForUrl(final String url, final double engagement) {
        ThreadUtils.runOnUiThreadBlocking(() -> {
            SiteEngagementService.getForProfile(Profile.getLastUsedProfile())
                    .resetBaseScoreForUrl(url, engagement);
        });
    }

    private void waitForBannerManager(Tab tab) {
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return !tab.getAppBannerManager().isRunningForTesting();
            }
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
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                AppBannerManager manager =
                        rule.getActivity().getActivityTab().getAppBannerManager();
                return mDetailsDelegate.mNumRetrieved == numExpected
                        && !manager.isRunningForTesting();
            }
        });
    }

    private void waitUntilAppBannerInfoBarAppears(
            ChromeActivityTestRule<? extends ChromeActivity> rule, final String title) {
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                List<InfoBar> infobars = rule.getInfoBars();
                if (infobars.size() != 1) return false;
                if (!(infobars.get(0) instanceof AppBannerInfoBarAndroid)) return false;

                TextView textView =
                        (TextView) infobars.get(0).getView().findViewById(R.id.infobar_message);
                if (textView == null) return false;
                return TextUtils.equals(textView.getText(), title);
            }
        });
    }

    private void waitUntilAmbientBadgeInfoBarAppears(
            ChromeActivityTestRule<? extends ChromeActivity> rule) {
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                List<InfoBar> infobars = rule.getInfoBars();
                if (infobars.size() != 1) return false;
                return infobars.get(0) instanceof InstallableAmbientBadgeInfoBar;
            }
        });
    }

    private void runFullNativeInstallPathway(
            String url, String expectedReferrer, String expectedTitle) throws Exception {
        // Say that the package isn't installed.
        Mockito.when(mPackageManager.getPackageInfo(
                             ArgumentMatchers.anyString(), ArgumentMatchers.anyInt()))
                .thenThrow(new PackageManager.NameNotFoundException());

        // Visit a site that requests a banner.
        Tab tab = mTabbedActivityTestRule.getActivity().getActivityTab();
        resetEngagementForUrl(url, 0);
        new TabLoadObserver(tab).fullyLoadUrl(url);
        InfoBarUtil.waitUntilNoInfoBarsExist(mTabbedActivityTestRule.getInfoBars());

        // Update engagement, then revisit the page to get the banner to appear.
        resetEngagementForUrl(url, 10);
        InfoBarContainer container = mTabbedActivityTestRule.getInfoBarContainer();
        final InfobarListener listener = new InfobarListener();
        container.addAnimationListener(listener);
        new TabLoadObserver(tab).fullyLoadUrl(url);
        waitUntilAppDetailsRetrieved(mTabbedActivityTestRule, 1);
        Assert.assertEquals(mDetailsDelegate.mReferrer, expectedReferrer);
        waitUntilAppBannerInfoBarAppears(mTabbedActivityTestRule, NATIVE_APP_TITLE);
        CriteriaHelper.pollUiThread(() -> listener.mDoneAnimating);

        // Check that the button asks if the user wants to install the app.
        InfoBar infobar = container.getInfoBarsForTesting().get(0);
        final Button button = (Button) infobar.getView().findViewById(R.id.button_primary);
        Assert.assertEquals(NATIVE_APP_INSTALL_TEXT, button.getText());

        // Click the button to trigger the install.
        final ActivityMonitor activityMonitor =
                new ActivityMonitor(new IntentFilter(INSTALL_ACTION),
                        new ActivityResult(Activity.RESULT_OK, null), true);
        InstrumentationRegistry.getInstrumentation().addMonitor(activityMonitor);
        TouchCommon.singleClickView(button);

        // Wait for the infobar to register that the app is installing.
        final String installingText = InstrumentationRegistry.getTargetContext().getString(
                R.string.app_banner_installing);
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return InstrumentationRegistry.getInstrumentation().checkMonitorHit(
                               activityMonitor, 1)
                        && TextUtils.equals(button.getText(), installingText);
            }
        });

        // If we expect an update of the page title via JavaScript, wait until the change happens.
        if (expectedTitle != null) {
            new TabTitleObserver(tab, expectedTitle).waitForTitleUpdate(3);
        }

        // Say that the package is installed.  Infobar should say that the app is ready to open.
        Mockito.reset(mPackageManager);
        PackageInfo info = new PackageInfo();
        info.packageName = NATIVE_APP_PACKAGE;
        Mockito.when(mPackageManager.getPackageInfo(
                             ArgumentMatchers.anyString(), ArgumentMatchers.anyInt()))
                .thenReturn(info);

        final String openText =
                InstrumentationRegistry.getTargetContext().getString(R.string.app_banner_open);
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return TextUtils.equals(button.getText(), openText);
            }
        });
    }

    private void triggerWebAppBanner(ChromeActivityTestRule<? extends ChromeActivity> rule,
            String url, String expectedTitle, boolean installApp) throws Exception {
        // Visit the site in a new tab.
        resetEngagementForUrl(url, 0);
        rule.loadUrlInNewTab(ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);
        navigateToUrlAndWaitForBannerManager(rule, url);
        InfoBarUtil.waitUntilNoInfoBarsExist(rule.getInfoBars());

        // Add the animation listener in.
        InfoBarContainer container = rule.getInfoBarContainer();
        final InfobarListener listener = new InfobarListener();
        container.addAnimationListener(listener);

        // Update engagement, then revisit the page to get the banner to appear.
        resetEngagementForUrl(url, 10);
        navigateToUrlAndWaitForBannerManager(rule, url);
        waitUntilAppBannerInfoBarAppears(rule, expectedTitle);

        if (!installApp) return;

        CriteriaHelper.pollUiThread(() -> listener.mDoneAnimating);

        // Click the button to trigger the adding of the shortcut.
        InfoBar infobar = container.getInfoBarsForTesting().get(0);
        final Button button = (Button) infobar.getView().findViewById(R.id.button_primary);
        TouchCommon.singleClickView(button);
    }

    private void blockInfoBarBannerAndResolveUserChoice(
            ChromeActivityTestRule<? extends ChromeActivity> rule, String url, String expectedTitle)
            throws Exception {
        // Update engagement, then visit a page which triggers a banner.
        Tab tab = rule.getActivity().getActivityTab();
        resetEngagementForUrl(url, 10);
        InfoBarContainer container = rule.getInfoBarContainer();
        final InfobarListener listener = new InfobarListener();
        container.addAnimationListener(listener);
        new TabLoadObserver(tab).fullyLoadUrl(url);
        if (expectedTitle.equals(NATIVE_APP_TITLE)) {
            waitUntilAppDetailsRetrieved(rule, 1);
        }
        waitUntilAppBannerInfoBarAppears(rule, expectedTitle);

        // Explicitly dismiss the banner.
        CriteriaHelper.pollUiThread(() -> listener.mDoneAnimating);
        ArrayList<InfoBar> infobars = container.getInfoBarsForTesting();
        View close = infobars.get(0).getView().findViewById(R.id.infobar_close_button);
        TouchCommon.singleClickView(close);

        InfoBarUtil.waitUntilNoInfoBarsExist(rule.getInfoBars());

        // Ensure userChoice is resolved.
        new TabTitleObserver(tab, "Got userChoice: dismissed").waitForTitleUpdate(3);
    }

    private void waitUntilNoDialogsShowing(final Tab tab) {
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                AddToHomescreenDialog dialog =
                        tab.getAppBannerManager().getAddToHomescreenDialogForTesting();
                return dialog == null || dialog.getAlertDialogForTesting() == null;
            }
        });
    }

    private void tapAndWaitForModalBanner(final Tab tab) throws Exception {
        TouchCommon.singleClickView(tab.getView());

        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                AddToHomescreenDialog dialog =
                        tab.getAppBannerManager().getAddToHomescreenDialogForTesting();
                if (dialog != null) {
                    AlertDialog alertDialog = dialog.getAlertDialogForTesting();
                    return alertDialog != null && alertDialog.isShowing();
                }
                return false;
            }
        });
    }

    private void triggerModalWebAppBanner(ChromeActivityTestRule<? extends ChromeActivity> rule,
            String url, boolean installApp) throws Exception {
        resetEngagementForUrl(url, 10);
        rule.loadUrlInNewTab(ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);
        navigateToUrlAndWaitForBannerManager(rule, url);
        waitUntilAmbientBadgeInfoBarAppears(rule);

        Tab tab = rule.getActivity().getActivityTab();
        tapAndWaitForModalBanner(tab);

        if (!installApp) return;

        // Click the button to trigger the adding of the shortcut.
        clickButton(tab, DialogInterface.BUTTON_POSITIVE);
    }

    private void triggerModalNativeAppBanner(ChromeActivityTestRule<? extends ChromeActivity> rule,
            String url, String expectedReferrer, boolean installApp) throws Exception {
        resetEngagementForUrl(url, 10);
        rule.loadUrlInNewTab(ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);
        navigateToUrlAndWaitForBannerManager(rule, url);
        waitUntilAppDetailsRetrieved(rule, 1);
        waitUntilAmbientBadgeInfoBarAppears(rule);
        Assert.assertEquals(mDetailsDelegate.mReferrer, expectedReferrer);

        final Tab tab = rule.getActivity().getActivityTab();
        tapAndWaitForModalBanner(tab);
        if (!installApp) return;

        // Click the button to trigger the installation.
        final ActivityMonitor activityMonitor =
                new ActivityMonitor(new IntentFilter(INSTALL_ACTION),
                        new ActivityResult(Activity.RESULT_OK, null), true);
        Instrumentation instrumentation = InstrumentationRegistry.getInstrumentation();
        instrumentation.addMonitor(activityMonitor);

        ThreadUtils.runOnUiThreadBlocking(() -> {
            Button button = tab.getAppBannerManager()
                                    .getAddToHomescreenDialogForTesting()
                                    .getAlertDialogForTesting()
                                    .getButton(DialogInterface.BUTTON_POSITIVE);
            Assert.assertEquals(NATIVE_APP_INSTALL_TEXT, button.getText());
        });

        clickButton(tab, DialogInterface.BUTTON_POSITIVE);

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

        waitUntilAmbientBadgeInfoBarAppears(rule);
        Tab tab = rule.getActivity().getActivityTab();
        tapAndWaitForModalBanner(tab);

        // Explicitly dismiss the banner. We should be able to show the banner after dismissing.
        clickButton(tab, DialogInterface.BUTTON_NEGATIVE);
        waitUntilNoDialogsShowing(tab);
        tapAndWaitForModalBanner(tab);

        clickButton(tab, DialogInterface.BUTTON_NEGATIVE);
        waitUntilNoDialogsShowing(tab);
        tapAndWaitForModalBanner(tab);
    }

    private void clickButton(final Tab tab, final int button) {
        ThreadUtils.runOnUiThreadBlocking(() -> {
            tab.getAppBannerManager()
                    .getAddToHomescreenDialogForTesting()
                    .getAlertDialogForTesting()
                    .getButton(button)
                    .performClick();
        });
    }

    @Test
    @SmallTest
    @Feature({"AppBanners"})
    public void testAppInstalledEventModalWebAppBannerBrowserTab() throws Exception {
        triggerModalWebAppBanner(mTabbedActivityTestRule,
                WebappTestPage.getServiceWorkerUrlWithAction(
                        mTestServer, "call_stashed_prompt_on_click_verify_appinstalled"),
                true);

        // The appinstalled event should fire (and cause the title to change).
        new TabTitleObserver(mTabbedActivityTestRule.getActivity().getActivityTab(),
                "Got appinstalled: listener, attr")
                .waitForTitleUpdate(3);
    }

    @Test
    @SmallTest
    @Feature({"AppBanners"})
    public void testAppInstalledEventModalWebAppBannerCustomTab() throws Exception {
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(
                CustomTabsTestUtils.createMinimalCustomTabIntent(
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

        // The appinstalled event should fire (and cause the title to change).
        new TabTitleObserver(mTabbedActivityTestRule.getActivity().getActivityTab(),
                "Got appinstalled: listener, attr")
                .waitForTitleUpdate(3);
    }

    @Test
    @SmallTest
    @Feature({"AppBanners"})
    public void testAppInstalledModalNativeAppBannerCustomTab() throws Exception {
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(
                CustomTabsTestUtils.createMinimalCustomTabIntent(
                        InstrumentationRegistry.getTargetContext(),
                        ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL));

        triggerModalNativeAppBanner(mCustomTabActivityTestRule,
                WebappTestPage.getNonServiceWorkerUrlWithManifestAndAction(mTestServer,
                        NATIVE_APP_MANIFEST_WITH_ID,
                        "call_stashed_prompt_on_click_verify_appinstalled"),
                NATIVE_APP_BLANK_REFERRER, true);

        // The appinstalled event should fire (and cause the title to change).
        new TabTitleObserver(mCustomTabActivityTestRule.getActivity().getActivityTab(),
                "Got appinstalled: listener, attr")
                .waitForTitleUpdate(3);
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
        final Tab tab = mTabbedActivityTestRule.getActivity().getActivityTab();
        clickButton(tab, DialogInterface.BUTTON_NEGATIVE);

        // Ensure userChoice is resolved.
        new TabTitleObserver(tab, "Got userChoice: dismissed").waitForTitleUpdate(3);
    }

    @Test
    @MediumTest
    @Feature({"AppBanners"})
    public void testBlockedModalNativeAppBannerResolveUserChoice() throws Exception {
        triggerModalNativeAppBanner(mTabbedActivityTestRule,
                WebappTestPage.getNonServiceWorkerUrlWithManifestAndAction(
                        mTestServer, NATIVE_APP_MANIFEST_WITH_ID, "call_stashed_prompt_on_click"),
                NATIVE_APP_BLANK_REFERRER, false);

        // Explicitly dismiss the banner.
        final Tab tab = mTabbedActivityTestRule.getActivity().getActivityTab();
        clickButton(tab, DialogInterface.BUTTON_NEGATIVE);

        // Ensure userChoice is resolved.
        new TabTitleObserver(tab, "Got userChoice: dismissed").waitForTitleUpdate(3);
    }

    @Test
    @SmallTest
    @Feature({"AppBanners"})
    public void testModalNativeAppBannerCanBeTriggeredMultipleTimesBrowserTab() throws Exception {
        triggerModalBannerMultipleTimes(mTabbedActivityTestRule,
                WebappTestPage.getNonServiceWorkerUrlWithManifestAndAction(
                        mTestServer, NATIVE_APP_MANIFEST_WITH_ID, "call_stashed_prompt_on_click"),
                true);
    }

    @Test
    @SmallTest
    @Feature({"AppBanners"})
    public void testModalNativeAppBannerCanBeTriggeredMultipleTimesCustomTab() throws Exception {
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(
                CustomTabsTestUtils.createMinimalCustomTabIntent(
                        InstrumentationRegistry.getTargetContext(),
                        ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL));

        triggerModalBannerMultipleTimes(mCustomTabActivityTestRule,
                WebappTestPage.getNonServiceWorkerUrlWithManifestAndAction(
                        mTestServer, NATIVE_APP_MANIFEST_WITH_ID, "call_stashed_prompt_on_click"),
                true);
    }

    @Test
    @MediumTest
    @Feature({"AppBanners"})
    public void testModalWebAppBannerCanBeTriggeredMultipleTimesBrowserTab() throws Exception {
        triggerModalBannerMultipleTimes(mTabbedActivityTestRule,
                WebappTestPage.getServiceWorkerUrlWithAction(
                        mTestServer, "call_stashed_prompt_on_click"),
                false);
    }

    @Test
    @SmallTest
    @Feature({"AppBanners"})
    public void testModalWebAppBannerCanBeTriggeredMultipleTimesCustomTab() throws Exception {
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(
                CustomTabsTestUtils.createMinimalCustomTabIntent(
                        InstrumentationRegistry.getTargetContext(),
                        ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL));

        triggerModalBannerMultipleTimes(mCustomTabActivityTestRule,
                WebappTestPage.getServiceWorkerUrlWithAction(
                        mTestServer, "call_stashed_prompt_on_click"),
                false);
    }

    @Test
    @MediumTest
    @Feature({"AppBanners"})
    public void testBlockedAmbientBadgeDoesNotAppearAgainForMonths() throws Exception {
        // Visit a site that is a PWA. The ambient badge should show.
        String webBannerUrl = WebappTestPage.getServiceWorkerUrl(mTestServer);

        Tab tab = mTabbedActivityTestRule.getActivity().getActivityTab();
        new TabLoadObserver(tab).fullyLoadUrl(webBannerUrl);
        waitUntilAmbientBadgeInfoBarAppears(mTabbedActivityTestRule);

        InfoBarContainer container = mTabbedActivityTestRule.getInfoBarContainer();
        final InfobarListener listener = new InfobarListener();
        container.addAnimationListener(listener);

        // Explicitly dismiss the ambient badge.
        CriteriaHelper.pollUiThread(() -> listener.mDoneAnimating);

        ArrayList<InfoBar> infobars = container.getInfoBarsForTesting();
        View close = infobars.get(0).getView().findViewById(R.id.infobar_close_button);
        TouchCommon.singleClickView(close);
        InfoBarUtil.waitUntilNoInfoBarsExist(mTabbedActivityTestRule.getInfoBars());

        // Waiting two months shouldn't be long enough.
        AppBannerManager.setTimeDeltaForTesting(61);
        new TabLoadObserver(tab).fullyLoadUrl(webBannerUrl);
        InfoBarUtil.waitUntilNoInfoBarsExist(mTabbedActivityTestRule.getInfoBars());

        AppBannerManager.setTimeDeltaForTesting(62);
        new TabLoadObserver(tab).fullyLoadUrl(webBannerUrl);
        InfoBarUtil.waitUntilNoInfoBarsExist(mTabbedActivityTestRule.getInfoBars());

        // Waiting three months should allow the ambient badge to reappear.
        AppBannerManager.setTimeDeltaForTesting(91);
        new TabLoadObserver(tab).fullyLoadUrl(webBannerUrl);
        waitUntilAmbientBadgeInfoBarAppears(mTabbedActivityTestRule);
    }

    @Test
    @SmallTest
    @Feature({"AppBanners"})
    @CommandLineFlags.Add("disable-features=" + ChromeFeatureList.EXPERIMENTAL_APP_BANNERS)
    public void testFullNativeInstallPathwayFromId() throws Exception {
        // Set the prompt handler so that the userChoice promise resolves and updates the title.
        runFullNativeInstallPathway(
                WebappTestPage.getNonServiceWorkerUrlWithManifestAndAction(
                        mTestServer, NATIVE_APP_MANIFEST_WITH_ID, "call_prompt_delayed"),
                NATIVE_APP_BLANK_REFERRER, "Got userChoice: accepted");
    }

    @Test
    @SmallTest
    @Feature({"AppBanners"})
    @CommandLineFlags.Add("disable-features=" + ChromeFeatureList.EXPERIMENTAL_APP_BANNERS)
    public void testFullNativeInstallPathwayFromUrl() throws Exception {
        runFullNativeInstallPathway(
                WebappTestPage.getNonServiceWorkerUrlWithManifestAndAction(
                        mTestServer, NATIVE_APP_MANIFEST_WITH_URL, "verify_appinstalled"),
                NATIVE_APP_REFERRER, "Got appinstalled: listener, attr");
    }

    @Test
    @MediumTest
    @Feature({"AppBanners"})
    @CommandLineFlags.Add("disable-features=" + ChromeFeatureList.EXPERIMENTAL_APP_BANNERS)
    public void testBannerAppearsThenDoesNotAppearAgainForWeeks() throws Exception {
        // Visit a site that requests a banner.
        Tab tab = mTabbedActivityTestRule.getActivity().getActivityTab();
        String nativeBannerUrl = WebappTestPage.getNonServiceWorkerUrlWithManifest(
                mTestServer, NATIVE_APP_MANIFEST_WITH_ID);
        resetEngagementForUrl(nativeBannerUrl, 0);
        new TabLoadObserver(tab).fullyLoadUrl(nativeBannerUrl);
        InfoBarUtil.waitUntilNoInfoBarsExist(mTabbedActivityTestRule.getInfoBars());

        // Update engagement, then revisit the page.
        resetEngagementForUrl(nativeBannerUrl, 10);
        new TabLoadObserver(tab).fullyLoadUrl(nativeBannerUrl);
        waitUntilAppDetailsRetrieved(mTabbedActivityTestRule, 1);
        waitUntilAppBannerInfoBarAppears(mTabbedActivityTestRule, NATIVE_APP_TITLE);

        // Revisit the page to make the banner go away, but don't explicitly dismiss it.
        // This hides the banner for two weeks.
        new TabLoadObserver(tab).fullyLoadUrl(nativeBannerUrl);
        waitUntilAppDetailsRetrieved(mTabbedActivityTestRule, 2);
        InfoBarUtil.waitUntilNoInfoBarsExist(mTabbedActivityTestRule.getInfoBars());

        // Wait a week until revisiting the page.
        AppBannerManager.setTimeDeltaForTesting(7);
        new TabLoadObserver(tab).fullyLoadUrl(nativeBannerUrl);
        waitUntilAppDetailsRetrieved(mTabbedActivityTestRule, 3);
        InfoBarUtil.waitUntilNoInfoBarsExist(mTabbedActivityTestRule.getInfoBars());

        AppBannerManager.setTimeDeltaForTesting(8);
        new TabLoadObserver(tab).fullyLoadUrl(nativeBannerUrl);
        waitUntilAppDetailsRetrieved(mTabbedActivityTestRule, 4);
        InfoBarUtil.waitUntilNoInfoBarsExist(mTabbedActivityTestRule.getInfoBars());

        // Wait two weeks until revisiting the page, which should pop up the banner.
        AppBannerManager.setTimeDeltaForTesting(15);
        new TabLoadObserver(tab).fullyLoadUrl(nativeBannerUrl);
        waitUntilAppDetailsRetrieved(mTabbedActivityTestRule, 5);
        waitUntilAppBannerInfoBarAppears(mTabbedActivityTestRule, NATIVE_APP_TITLE);
    }

    @Test
    @MediumTest
    @Feature({"AppBanners"})
    @CommandLineFlags.Add("disable-features=" + ChromeFeatureList.EXPERIMENTAL_APP_BANNERS)
    public void testBannerAppearsThenDoesNotAppearAgainForCustomTime() throws Exception {
        AppBannerManager.setDaysAfterDismissAndIgnoreForTesting(7, 7);
        String webBannerUrl = WebappTestPage.getServiceWorkerUrl(mTestServer);
        triggerWebAppBanner(mTabbedActivityTestRule, webBannerUrl, WEB_APP_TITLE, false);

        // Revisit the page to make the banner go away, but don't explicitly dismiss it.
        // This hides the banner for two weeks.
        Tab tab = mTabbedActivityTestRule.getActivity().getActivityTab();
        new TabLoadObserver(tab).fullyLoadUrl(webBannerUrl);
        InfoBarUtil.waitUntilNoInfoBarsExist(mTabbedActivityTestRule.getInfoBars());

        // Wait a week until revisiting the page. This should allow the banner.
        AppBannerManager.setTimeDeltaForTesting(7);
        new TabLoadObserver(tab).fullyLoadUrl(webBannerUrl);
        waitUntilAppBannerInfoBarAppears(mTabbedActivityTestRule, WEB_APP_TITLE);
    }

    @Test
    @MediumTest
    @Feature({"AppBanners"})
    @CommandLineFlags.Add("disable-features=" + ChromeFeatureList.EXPERIMENTAL_APP_BANNERS)
    public void testBlockedBannerDoesNotAppearAgainForMonths() throws Exception {
        // Visit a site that requests a banner.
        String nativeBannerUrl = WebappTestPage.getNonServiceWorkerUrlWithManifest(
                mTestServer, NATIVE_APP_MANIFEST_WITH_ID);
        resetEngagementForUrl(nativeBannerUrl, 0);

        Tab tab = mTabbedActivityTestRule.getActivity().getActivityTab();
        new TabLoadObserver(tab).fullyLoadUrl(nativeBannerUrl);
        InfoBarUtil.waitUntilNoInfoBarsExist(mTabbedActivityTestRule.getInfoBars());

        // Update engagement, then revisit the page.
        resetEngagementForUrl(nativeBannerUrl, 10);
        InfoBarContainer container = mTabbedActivityTestRule.getInfoBarContainer();
        final InfobarListener listener = new InfobarListener();
        container.addAnimationListener(listener);
        new TabLoadObserver(tab).fullyLoadUrl(nativeBannerUrl);
        waitUntilAppDetailsRetrieved(mTabbedActivityTestRule, 1);
        waitUntilAppBannerInfoBarAppears(mTabbedActivityTestRule, NATIVE_APP_TITLE);

        // Explicitly dismiss the banner.
        CriteriaHelper.pollUiThread(() -> listener.mDoneAnimating);
        ArrayList<InfoBar> infobars = container.getInfoBarsForTesting();
        View close = infobars.get(0).getView().findViewById(R.id.infobar_close_button);
        TouchCommon.singleClickView(close);
        InfoBarUtil.waitUntilNoInfoBarsExist(mTabbedActivityTestRule.getInfoBars());

        // Waiting two months shouldn't be long enough.
        AppBannerManager.setTimeDeltaForTesting(61);
        new TabLoadObserver(tab).fullyLoadUrl(nativeBannerUrl);
        waitUntilAppDetailsRetrieved(mTabbedActivityTestRule, 2);
        InfoBarUtil.waitUntilNoInfoBarsExist(mTabbedActivityTestRule.getInfoBars());

        AppBannerManager.setTimeDeltaForTesting(62);
        new TabLoadObserver(tab).fullyLoadUrl(nativeBannerUrl);
        waitUntilAppDetailsRetrieved(mTabbedActivityTestRule, 3);
        InfoBarUtil.waitUntilNoInfoBarsExist(mTabbedActivityTestRule.getInfoBars());

        // Waiting three months should allow banners to reappear.
        AppBannerManager.setTimeDeltaForTesting(91);
        new TabLoadObserver(tab).fullyLoadUrl(nativeBannerUrl);
        waitUntilAppDetailsRetrieved(mTabbedActivityTestRule, 4);
        waitUntilAppBannerInfoBarAppears(mTabbedActivityTestRule, NATIVE_APP_TITLE);
    }

    @Test
    @MediumTest
    @Feature({"AppBanners"})
    @CommandLineFlags.Add("disable-features=" + ChromeFeatureList.EXPERIMENTAL_APP_BANNERS)
    public void testBlockedBannerDoesNotAppearAgainForCustomTime() throws Exception {
        AppBannerManager.setDaysAfterDismissAndIgnoreForTesting(7, 7);
        String webBannerUrl = WebappTestPage.getServiceWorkerUrl(mTestServer);
        Tab tab = mTabbedActivityTestRule.getActivity().getActivityTab();

        // Update engagement, then visit a page which triggers a banner.
        resetEngagementForUrl(webBannerUrl, 10);
        InfoBarContainer container = mTabbedActivityTestRule.getInfoBarContainer();
        final InfobarListener listener = new InfobarListener();
        container.addAnimationListener(listener);
        new TabLoadObserver(tab).fullyLoadUrl(webBannerUrl);
        waitUntilAppBannerInfoBarAppears(mTabbedActivityTestRule, WEB_APP_TITLE);

        // Explicitly dismiss the banner.
        CriteriaHelper.pollUiThread(() -> listener.mDoneAnimating);
        ArrayList<InfoBar> infobars = container.getInfoBarsForTesting();
        View close = infobars.get(0).getView().findViewById(R.id.infobar_close_button);
        TouchCommon.singleClickView(close);
        InfoBarUtil.waitUntilNoInfoBarsExist(mTabbedActivityTestRule.getInfoBars());

        // Waiting seven days should be long enough.
        AppBannerManager.setTimeDeltaForTesting(7);
        new TabLoadObserver(tab).fullyLoadUrl(webBannerUrl);
        waitUntilAppBannerInfoBarAppears(mTabbedActivityTestRule, WEB_APP_TITLE);
    }

    @Test
    @MediumTest
    @Feature({"AppBanners"})
    @CommandLineFlags.Add("disable-features=" + ChromeFeatureList.EXPERIMENTAL_APP_BANNERS)
    public void testBlockedWebAppBannerBrowserTabResolvesUserChoice() throws Exception {
        blockInfoBarBannerAndResolveUserChoice(mTabbedActivityTestRule,
                WebappTestPage.getServiceWorkerUrlWithAction(mTestServer, "call_prompt_delayed"),
                WEB_APP_TITLE);
    }

    @Test
    @MediumTest
    @Feature({"AppBanners"})
    @CommandLineFlags.Add("disable-features=" + ChromeFeatureList.EXPERIMENTAL_APP_BANNERS)
    public void testBlockedNativeAppBannerBrowserTabResolvesUserChoice() throws Exception {
        blockInfoBarBannerAndResolveUserChoice(mTabbedActivityTestRule,
                WebappTestPage.getNonServiceWorkerUrlWithManifestAndAction(
                        mTestServer, NATIVE_APP_MANIFEST_WITH_ID, "call_prompt_delayed"),
                NATIVE_APP_TITLE);
    }

    @Test
    @MediumTest
    @Feature({"AppBanners"})
    @CommandLineFlags.Add("disable-features=" + ChromeFeatureList.EXPERIMENTAL_APP_BANNERS)
    public void testBlockedWebAppBannerCustomTabResolvesUserChoice() throws Exception {
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(
                CustomTabsTestUtils.createMinimalCustomTabIntent(
                        InstrumentationRegistry.getTargetContext(),
                        ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL));
        blockInfoBarBannerAndResolveUserChoice(mCustomTabActivityTestRule,
                WebappTestPage.getServiceWorkerUrlWithAction(mTestServer, "call_prompt_delayed"),
                WEB_APP_TITLE);
    }

    @Test
    @MediumTest
    @Feature({"AppBanners"})
    @CommandLineFlags.Add("disable-features=" + ChromeFeatureList.EXPERIMENTAL_APP_BANNERS)
    public void testBlockedNativeAppBannerResolvesUserChoice() throws Exception {
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(
                CustomTabsTestUtils.createMinimalCustomTabIntent(
                        InstrumentationRegistry.getTargetContext(),
                        ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL));
        blockInfoBarBannerAndResolveUserChoice(mCustomTabActivityTestRule,
                WebappTestPage.getNonServiceWorkerUrlWithManifestAndAction(
                        mTestServer, NATIVE_APP_MANIFEST_WITH_ID, "call_prompt_delayed"),
                NATIVE_APP_TITLE);
    }

    @Test
    @MediumTest
    @Feature({"AppBanners"})
    @CommandLineFlags.Add("disable-features=" + ChromeFeatureList.EXPERIMENTAL_APP_BANNERS)
    public void testBitmapFetchersCanOverlapWithoutCrashing() throws Exception {
        // Visit a site that requests a banner rapidly and repeatedly.
        String nativeBannerUrl = WebappTestPage.getNonServiceWorkerUrlWithManifest(
                mTestServer, NATIVE_APP_MANIFEST_WITH_ID);
        resetEngagementForUrl(nativeBannerUrl, 10);
        for (int i = 1; i <= 10; i++) {
            new TabLoadObserver(mTabbedActivityTestRule.getActivity().getActivityTab())
                    .fullyLoadUrl(nativeBannerUrl);

            final Integer iteration = Integer.valueOf(i);
            CriteriaHelper.pollUiThread(Criteria.equals(iteration, new Callable<Integer>() {
                @Override
                public Integer call() {
                    return mDetailsDelegate.mNumRetrieved;
                }
            }));
        }
    }

    @Test
    @SmallTest
    @Feature({"AppBanners"})
    @CommandLineFlags.Add("disable-features=" + ChromeFeatureList.EXPERIMENTAL_APP_BANNERS)
    public void testWebAppBannerAppears() throws Exception {
        String webBannerUrl = WebappTestPage.getServiceWorkerUrl(mTestServer);
        triggerWebAppBanner(mTabbedActivityTestRule, webBannerUrl, WEB_APP_TITLE, false);

        // Verify metrics calling in the successful case.
        ThreadUtils.runOnUiThread(() -> {
            AppBannerManager manager =
                    mTabbedActivityTestRule.getActivity().getActivityTab().getAppBannerManager();
            manager.recordMenuItemAddToHomescreen();
            Assert.assertEquals(1,
                    RecordHistogram.getHistogramValueCountForTesting(
                            "Webapp.InstallabilityCheckStatus.MenuItemAddToHomescreen", 5));

            manager.recordMenuOpen();
            Assert.assertEquals(1,
                    RecordHistogram.getHistogramValueCountForTesting(
                            "Webapp.InstallabilityCheckStatus.MenuOpen", 5));
        });
    }

    @Test
    @SmallTest
    @Feature({"AppBanners"})
    @CommandLineFlags.Add("disable-features=" + ChromeFeatureList.EXPERIMENTAL_APP_BANNERS)
    public void testWebAppBannerDoesNotAppearAfterInstall() throws Exception {
        String webBannerUrl = WebappTestPage.getServiceWorkerUrl(mTestServer);
        triggerWebAppBanner(mTabbedActivityTestRule, webBannerUrl, WEB_APP_TITLE, true);

        // The banner should not reshow after the site has been installed.
        AppBannerManager.setTimeDeltaForTesting(100);
        new TabLoadObserver(mTabbedActivityTestRule.getActivity().getActivityTab())
                .fullyLoadUrl(webBannerUrl);
        InfoBarUtil.waitUntilNoInfoBarsExist(mTabbedActivityTestRule.getInfoBars());
    }

    @Test
    @SmallTest
    @Feature({"AppBanners"})
    @CommandLineFlags.Add("disable-features=" + ChromeFeatureList.EXPERIMENTAL_APP_BANNERS)
    public void testBannerFallsBackToShortNameWhenNameNotPresent() throws Exception {
        triggerWebAppBanner(mTabbedActivityTestRule,
                WebappTestPage.getServiceWorkerUrlWithManifest(
                        mTestServer, WEB_APP_SHORT_TITLE_MANIFEST),
                WEB_APP_SHORT_TITLE, false);
    }

    @Test
    @SmallTest
    @Feature({"AppBanners"})
    @CommandLineFlags.Add("disable-features=" + ChromeFeatureList.EXPERIMENTAL_APP_BANNERS)
    public void testBannerFallsBackToShortNameWhenNameIsEmpty() throws Exception {
        triggerWebAppBanner(mTabbedActivityTestRule,
                WebappTestPage.getServiceWorkerUrlWithManifest(
                        mTestServer, WEB_APP_EMPTY_NAME_MANIFEST),
                WEB_APP_SHORT_TITLE, false);
    }

    @Test
    @MediumTest
    @Feature({"AppBanners"})
    @CommandLineFlags.Add("disable-features=" + ChromeFeatureList.EXPERIMENTAL_APP_BANNERS)
    public void testAppInstalledEventAutomaticPrompt() throws Exception {
        triggerWebAppBanner(mTabbedActivityTestRule,
                WebappTestPage.getServiceWorkerUrlWithAction(mTestServer, "verify_appinstalled"),
                WEB_APP_TITLE, true);

        // The appinstalled event should fire (and cause the title to change).
        new TabTitleObserver(mTabbedActivityTestRule.getActivity().getActivityTab(),
                "Got appinstalled: listener, attr")
                .waitForTitleUpdate(3);
    }

    @Test
    @MediumTest
    @Feature({"AppBanners"})
    @CommandLineFlags.Add("disable-features=" + ChromeFeatureList.EXPERIMENTAL_APP_BANNERS)
    public void testAppInstalledEventApi() throws Exception {
        triggerWebAppBanner(mTabbedActivityTestRule,
                WebappTestPage.getServiceWorkerUrlWithAction(
                        mTestServer, "verify_prompt_appinstalled"),
                WEB_APP_TITLE, true);

        // The appinstalled event should fire (and cause the title to change).
        new TabTitleObserver(mTabbedActivityTestRule.getActivity().getActivityTab(),
                "Got appinstalled: listener, attr")
                .waitForTitleUpdate(3);
    }

    @Test
    @SmallTest
    @Feature({"AppBanners"})
    @CommandLineFlags.Add("disable-features=" + ChromeFeatureList.EXPERIMENTAL_APP_BANNERS)
    public void testPostInstallationAutomaticPromptBrowserTab() throws Exception {
        String webBannerUrl = WebappTestPage.getServiceWorkerUrl(mTestServer);
        triggerWebAppBanner(mTabbedActivityTestRule, webBannerUrl, WEB_APP_TITLE, true);

        ThreadUtils.runOnUiThread(() -> {
            Assert.assertEquals(1,
                    RecordHistogram.getHistogramValueCountForTesting(
                            "Webapp.Install.InstallEvent", 2));
        });
    }

    @Test
    @SmallTest
    @Feature({"AppBanners"})
    @CommandLineFlags.Add("disable-features=" + ChromeFeatureList.EXPERIMENTAL_APP_BANNERS)
    public void testPostInstallationAutomaticPromptCustomTab() throws Exception {
        String webBannerUrl = WebappTestPage.getServiceWorkerUrl(mTestServer);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(
                CustomTabsTestUtils.createMinimalCustomTabIntent(
                        InstrumentationRegistry.getTargetContext(),
                        ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL));
        triggerWebAppBanner(mCustomTabActivityTestRule, webBannerUrl, WEB_APP_TITLE, true);

        ThreadUtils.runOnUiThread(() -> {
            Assert.assertEquals(1,
                    RecordHistogram.getHistogramValueCountForTesting(
                            "Webapp.Install.InstallEvent", 3));
        });
    }

    @Test
    @SmallTest
    @Feature({"AppBanners"})
    @CommandLineFlags.Add("disable-features=" + ChromeFeatureList.EXPERIMENTAL_APP_BANNERS)
    public void testPostInstallationApiBrowserTab() throws Exception {
        triggerWebAppBanner(mTabbedActivityTestRule,
                WebappTestPage.getServiceWorkerUrlWithAction(mTestServer, "call_prompt_delayed"),
                WEB_APP_TITLE, true);

        new TabTitleObserver(
                mTabbedActivityTestRule.getActivity().getActivityTab(), "Got userChoice: accepted")
                .waitForTitleUpdate(3);

        ThreadUtils.runOnUiThread(() -> {
            Assert.assertEquals(1,
                    RecordHistogram.getHistogramValueCountForTesting(
                            "Webapp.Install.InstallEvent", 4));
        });
    }

    @Test
    @SmallTest
    @Feature({"AppBanners"})
    @CommandLineFlags.Add("disable-features=" + ChromeFeatureList.EXPERIMENTAL_APP_BANNERS)
    public void testPostInstallationApiCustomTab() throws Exception {
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(
                CustomTabsTestUtils.createMinimalCustomTabIntent(
                        InstrumentationRegistry.getTargetContext(),
                        ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL));
        triggerWebAppBanner(mCustomTabActivityTestRule,
                WebappTestPage.getServiceWorkerUrlWithAction(mTestServer, "call_prompt_delayed"),
                WEB_APP_TITLE, true);

        new TabTitleObserver(mCustomTabActivityTestRule.getActivity().getActivityTab(),
                "Got userChoice: accepted")
                .waitForTitleUpdate(3);

        ThreadUtils.runOnUiThread(() -> {
            Assert.assertEquals(1,
                    RecordHistogram.getHistogramValueCountForTesting(
                            "Webapp.Install.InstallEvent", 5));
        });
    }

    @Test
    @SmallTest
    @Feature({"AppBanners"})
    @CommandLineFlags.Add("disable-features=" + ChromeFeatureList.EXPERIMENTAL_APP_BANNERS)
    public void testBannerAppearsImmediatelyWithSufficientEngagement() throws Exception {
        // Visit the site in a new tab with sufficient engagement and verify it appears.
        String webBannerUrl = WebappTestPage.getServiceWorkerUrl(mTestServer);
        resetEngagementForUrl(webBannerUrl, 10);
        mTabbedActivityTestRule.loadUrlInNewTab(ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);

        navigateToUrlAndWaitForBannerManager(mTabbedActivityTestRule, webBannerUrl);
        waitUntilAppBannerInfoBarAppears(mTabbedActivityTestRule, WEB_APP_TITLE);
    }

    @Test
    @SmallTest
    @Feature({"AppBanners"})
    @CommandLineFlags.Add("disable-features=" + ChromeFeatureList.EXPERIMENTAL_APP_BANNERS)
    public void testBannerDoesNotAppearInIncognito() throws Exception {
        // Visit the site in an incognito tab and verify it doesn't appear.
        String webBannerUrl = WebappTestPage.getServiceWorkerUrl(mTestServer);
        resetEngagementForUrl(webBannerUrl, 10);
        mTabbedActivityTestRule.loadUrlInNewTab(ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL, true);

        navigateToUrlAndWaitForBannerManager(mTabbedActivityTestRule, webBannerUrl);
        Assert.assertTrue(mTabbedActivityTestRule.getInfoBars().isEmpty());
    }

    @Test
    @SmallTest
    @Feature({"AppBanners"})
    @CommandLineFlags.Add("disable-features=" + ChromeFeatureList.EXPERIMENTAL_APP_BANNERS)
    public void testWebAppSplashscreenIsDownloaded() throws Exception {
        // Sets the overriden factory to observer splash screen update.
        final TestDataStorageFactory dataStorageFactory = new TestDataStorageFactory();
        WebappDataStorage.setFactoryForTests(dataStorageFactory);

        String webBannerUrl = WebappTestPage.getServiceWorkerUrl(mTestServer);
        triggerWebAppBanner(mTabbedActivityTestRule, webBannerUrl, WEB_APP_TITLE, true);

        // Make sure that the splash screen icon was downloaded.
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return dataStorageFactory.mSplashImage != null;
            }
        });

        // Test that bitmap sizes match expectations.
        int idealSize = mTabbedActivityTestRule.getActivity().getResources().getDimensionPixelSize(
                R.dimen.webapp_splash_image_size_ideal);
        Bitmap splashImage = ShortcutHelper.decodeBitmapFromString(dataStorageFactory.mSplashImage);
        Assert.assertEquals(idealSize, splashImage.getWidth());
        Assert.assertEquals(idealSize, splashImage.getHeight());
    }
}
