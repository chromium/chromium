// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.portals;

import android.app.NotificationManager;
import android.content.Context;
import android.service.notification.StatusBarNotification;
import android.text.TextUtils;
import android.view.View;

import androidx.test.InstrumentationRegistry;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.LargeTest;
import androidx.test.filters.MediumTest;
import androidx.test.uiautomator.UiDevice;
import androidx.test.uiautomator.UiObject;
import androidx.test.uiautomator.UiSelector;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.history.BrowsingHistoryBridge;
import org.chromium.chrome.browser.history.HistoryItem;
import org.chromium.chrome.browser.history.TestBrowsingHistoryObserver;
import org.chromium.chrome.browser.login.ChromeHttpAuthHandler;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.webapps.WebappTestPage;
import org.chromium.components.location.LocationUtils;
import org.chromium.components.permissions.PermissionDialogController;
import org.chromium.components.site_engagement.SiteEngagementService;
import org.chromium.components.webapps.AppBannerManager;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.Coordinates;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.modaldialog.ModalDialogProperties;

import java.util.List;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * Tests for the chrome/ layer support of the HTML portal element.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, "enable-features=Portals",
        "enable-blink-features=OverscrollCustomization"})
public class PortalsTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private EmbeddedTestServer mTestServer;

    @Before
    public void setUp() {
        mTestServer = EmbeddedTestServer.createAndStartServer(
                ApplicationProvider.getApplicationContext());
        // Some devices have geolocation disabled, so override LocationUtils to always show
        // geolocation as enabled to prevent flakes in the permissions tests.
        LocationUtils.setFactory(() -> new LocationUtils() {
            @Override
            public boolean isSystemLocationSettingEnabled() {
                return true;
            }
        });
    }

    @After
    public void tearDown() {
        mTestServer.stopAndDestroyServer();
        LocationUtils.setFactory(null);
    }

    private class TabContentsSwapObserver extends EmptyTabObserver {
        private final CallbackHelper mCallbackHelper;

        public TabContentsSwapObserver() {
            mCallbackHelper = new CallbackHelper();
        }

        public CallbackHelper getCallbackHelper() {
            return mCallbackHelper;
        }

        @Override
        public void onWebContentsSwapped(Tab tab, boolean didStartLoad, boolean didFinishLoad) {
            mCallbackHelper.notifyCalled();
        }
    }

    /**
     * Used to observe and wait for the first layout after the tab's WebContents is swapped.
     */
    private class LayoutAfterTabContentsSwappedObserver
            extends EmptyTabObserver implements View.OnLayoutChangeListener {
        private final CallbackHelper mCallbackHelper;

        public LayoutAfterTabContentsSwappedObserver(Tab tab) {
            TestThreadUtils.runOnUiThreadBlocking(() -> tab.addObserver(this));
            mCallbackHelper = new CallbackHelper();
        }

        @Override
        public void onWebContentsSwapped(Tab tab, boolean didStartLoad, boolean didFinishLoad) {
            tab.getContentView().addOnLayoutChangeListener(this);
        }

        public CallbackHelper getCallbackHelper() {
            return mCallbackHelper;
        }

        @Override
        public void onLayoutChange(View v, int left, int top, int right, int bottom, int oldLeft,
                int oldTop, int oldRight, int oldBottom) {
            v.removeOnLayoutChangeListener(this);
            mCallbackHelper.notifyCalled();
        }
    }

    private void executeScriptAndAwaitSwap(Tab tab, String code) throws Exception {
        TabContentsSwapObserver swapObserver = new TabContentsSwapObserver();
        CallbackHelper swapWaiter = swapObserver.getCallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(() -> tab.addObserver(swapObserver));

        int currSwapCount = swapWaiter.getCallCount();
        JavaScriptUtils.executeJavaScript(tab.getWebContents(), code);
        swapWaiter.waitForCallback(currSwapCount, 1);
    }

    private List<HistoryItem> getBrowsingHistory(Tab tab) throws TimeoutException {
        TestBrowsingHistoryObserver observer = new TestBrowsingHistoryObserver();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Profile profile = Profile.fromWebContents(tab.getWebContents());
            BrowsingHistoryBridge provider = new BrowsingHistoryBridge(profile);
            provider.setObserver(observer);
            provider.queryHistory(/* query */ "");
        });
        observer.getQueryCallback().waitForCallback(0);
        return observer.getHistoryQueryResults();
    }

    private WebContents getPortalContents(Tab tab) throws ExecutionException {
        final WebContents portalContents = TestThreadUtils.runOnUiThreadBlocking(() -> {
            List<? extends WebContents> innerContents = tab.getWebContents().getInnerWebContents();
            Assert.assertEquals(1, innerContents.size());
            return innerContents.get(0);
        });
        return portalContents;
    }

    /**
     * Tests that a portal can be activated and have its contents swapped in to its embedder's tab.
     */
    @Test
    @MediumTest
    @Feature({"Portals"})
    public void testActivate() throws Exception {
        mActivityTestRule.startMainActivityWithURL(mTestServer.getURL(
                "/chrome/test/data/android/portals/portal-to-basic-content.html"));

        final Tab tab = mActivityTestRule.getActivity().getActivityTab();

        final WebContents embedderContents = tab.getWebContents();
        Assert.assertNotNull(embedderContents);

        executeScriptAndAwaitSwap(tab, "activatePortal();");

        final WebContents portalContents = tab.getWebContents();
        Assert.assertNotNull(portalContents);
        Assert.assertNotSame(embedderContents, portalContents);
    }

    /**
     * Tests that a portal can be activated and adopt the embedder's contents.
     */
    @Test
    @MediumTest
    @Feature({"Portals"})
    public void testAdoptPredecessor() throws Exception {
        mActivityTestRule.startMainActivityWithURL(
                mTestServer.getURL("/chrome/test/data/android/portals/predecessor-adoption.html"));

        final Tab tab = mActivityTestRule.getActivity().getActivityTab();

        executeScriptAndAwaitSwap(tab, "activatePortal();");
        JavaScriptUtils.runJavascriptWithAsyncResult(tab.getWebContents(),
                "pingPredecessor().then(() => { domAutomationController.send(true); });");
    }

    /**
     * Tests that an adopted predecessor can be activated and adopt its portal back.
     */
    @Test
    @MediumTest
    @Feature({"Portals"})
    public void testReactivatePredecessor() throws Exception {
        mActivityTestRule.startMainActivityWithURL(
                mTestServer.getURL("/chrome/test/data/android/portals/predecessor-adoption.html"));

        final Tab tab = mActivityTestRule.getActivity().getActivityTab();

        executeScriptAndAwaitSwap(tab, "activatePortal();");
        executeScriptAndAwaitSwap(tab, "reactivatePredecessor();");
    }

    /**
     * Tests that an adopted predecessor can be destroyed.
     */
    @Test
    @MediumTest
    @Feature({"Portals"})
    public void testRemovePredecessor() throws Exception {
        mActivityTestRule.startMainActivityWithURL(
                mTestServer.getURL("/chrome/test/data/android/portals/predecessor-adoption.html"));

        final Tab tab = mActivityTestRule.getActivity().getActivityTab();

        executeScriptAndAwaitSwap(tab, "activatePortal();");
        JavaScriptUtils.executeJavaScriptAndWaitForResult(
                tab.getWebContents(), "removePredecessor();");
    }

    /**
     * Tests that a previously activated portal can be destroyed.
     */
    @Test
    @MediumTest
    @Feature({"Portals"})
    public void testRemovePreviouslyActivePortal() throws Exception {
        mActivityTestRule.startMainActivityWithURL(
                mTestServer.getURL("/chrome/test/data/android/portals/predecessor-adoption.html"));

        final Tab tab = mActivityTestRule.getActivity().getActivityTab();

        executeScriptAndAwaitSwap(tab, "activatePortal();");
        executeScriptAndAwaitSwap(tab, "reactivatePredecessor();");
        JavaScriptUtils.executeJavaScriptAndWaitForResult(tab.getWebContents(), "removePortal();");
    }

    @Test
    @MediumTest
    @Feature({"Portals"})
    public void testFocusTransfersAcrossActivation() throws Exception {
        mActivityTestRule.startMainActivityWithURL(
                mTestServer.getURL("/chrome/test/data/android/portals/focus-transfer.html"));
        final Tab tab = mActivityTestRule.getActivity().getActivityTab();
        executeScriptAndAwaitSwap(tab, "activatePortal()");
        JavaScriptUtils.runJavascriptWithAsyncResult(tab.getWebContents(),
                "focusPromise.then(() => domAutomationController.send(true));");
        Assert.assertEquals("true",
                JavaScriptUtils.runJavascriptWithAsyncResult(
                        tab.getWebContents(), "windowBlurred()"));
        Assert.assertEquals("true",
                JavaScriptUtils.runJavascriptWithAsyncResult(
                        tab.getWebContents(), "buttonBlurred()"));
    }

    @Test
    @MediumTest
    @Feature({"Portals"})
    public void testAutofocusAcrossActivation() throws Exception {
        mActivityTestRule.startMainActivityWithURL(
                mTestServer.getURL("/chrome/test/data/portal/autofocus.html"));
        final Tab tab = mActivityTestRule.getActivity().getActivityTab();
        final WebContents portalContents = getPortalContents(tab);
        // Elements with autofocus are only focused after a rendering update, so we call
        // requestAnimationFrame() to wait for the update.
        JavaScriptUtils.runJavascriptWithAsyncResult(
                portalContents, "rAF().then(() => domAutomationController.send(true));");
        Assert.assertEquals("true",
                JavaScriptUtils.executeJavaScriptAndWaitForResult(
                        portalContents, "checkActiveElement()"));
        Assert.assertEquals("false",
                JavaScriptUtils.executeJavaScriptAndWaitForResult(
                        portalContents, "focusEventDispatched"));
        executeScriptAndAwaitSwap(tab, "activate()");
        JavaScriptUtils.runJavascriptWithAsyncResult(tab.getWebContents(),
                "focusPromise.then(() => domAutomationController.send(true));");
    }

    /**
     * Tests that a drag that started in the predecessor page causes a scroll in the activated page
     * after a scroll triggered activation.
     */
    @Test
    @MediumTest
    @Feature({"Portals"})
    @DisabledTest(message = "https://crbug.com/1024850")
    public void testTouchTransfer() throws Exception {
        mActivityTestRule.startMainActivityWithURL(mTestServer.getURL(
                "/chrome/test/data/android/portals/touch-transfer.html?event=overscroll"));

        ChromeActivity activity = mActivityTestRule.getActivity();
        Tab tab = activity.getActivityTab();
        View contentView = tab.getContentView();
        LayoutAfterTabContentsSwappedObserver layoutObserver =
                new LayoutAfterTabContentsSwappedObserver(tab);
        CallbackHelper layoutWaiter = layoutObserver.getCallbackHelper();
        int currLayoutCount = layoutWaiter.getCallCount();

        int dragStartX = 30;
        int dragStartY = contentView.getHeight() / 2;
        int dragPauseY = dragStartY - 30;
        int dragEndX = dragStartX;
        int dragEndY = 30;
        long downTime = System.currentTimeMillis();

        // Initial drag to trigger activation.
        TouchCommon.dragStart(activity, dragStartX, dragStartY, downTime);
        TouchCommon.dragTo(activity, dragStartX, dragEndX, dragStartY, dragPauseY, 100, downTime);

        // Wait for the first layout after tab contents are swapped. This is needed as touch events
        // sent before the first layout are dropped.
        layoutWaiter.waitForCallback(currLayoutCount, 1);

        // Continue and finish drag.
        TouchCommon.dragTo(activity, dragStartX, dragEndX, dragPauseY, dragEndY, 100, downTime);
        TouchCommon.dragEnd(activity, dragEndX, dragEndY, downTime);

        WebContents contents = mActivityTestRule.getWebContents();
        Assert.assertTrue(Coordinates.createFor(contents).getScrollYPixInt() > 0);
    }

    /**
     * Tests that touch is transferred after triggering portal activation on touchstart.
     */
    @Test
    @MediumTest
    @Feature({"Portals"})
    @DisabledTest(message = "https://crbug.com/1024850")
    public void testTouchTransferAfterTouchStartActivate() throws Exception {
        mActivityTestRule.startMainActivityWithURL(mTestServer.getURL(
                "/chrome/test/data/android/portals/touch-transfer.html?event=touchstart"));

        ChromeActivity activity = mActivityTestRule.getActivity();
        Tab tab = activity.getActivityTab();
        View contentView = tab.getContentView();
        LayoutAfterTabContentsSwappedObserver layoutObserver =
                new LayoutAfterTabContentsSwappedObserver(tab);
        CallbackHelper layoutWaiter = layoutObserver.getCallbackHelper();
        int currLayoutCount = layoutWaiter.getCallCount();

        int dragStartX = 30;
        int dragStartY = contentView.getHeight() / 2;
        int dragEndX = dragStartX;
        int dragEndY = 30;
        long downTime = System.currentTimeMillis();

        // Initial touch to trigger activation.
        TouchCommon.dragStart(activity, dragStartX, dragStartY, downTime);

        // Wait for the first layout after tab contents are swapped. This is needed as touch events
        // sent before the first layout are dropped.
        layoutWaiter.waitForCallback(currLayoutCount, 1);

        // Continue and finish drag.
        TouchCommon.dragTo(activity, dragStartX, dragEndX, dragStartY, dragEndY, 100, downTime);
        TouchCommon.dragEnd(activity, dragEndX, dragEndY, downTime);

        WebContents contents = mActivityTestRule.getWebContents();
        Assert.assertTrue(Coordinates.createFor(contents).getScrollYPixInt() > 0);
    }

    @Test
    @MediumTest
    @Feature({"Portals"})
    @DisabledTest(message = "https://crbug.com/1024850")
    public void testTouchTransferAfterReactivation() throws Exception {
        mActivityTestRule.startMainActivityWithURL(mTestServer.getURL(
                "/chrome/test/data/android/portals/touch-transfer-after-reactivation.html"));

        ChromeActivity activity = mActivityTestRule.getActivity();
        Tab tab = activity.getActivityTab();
        View contentView = tab.getContentView();

        TabContentsSwapObserver swapObserver = new TabContentsSwapObserver();
        CallbackHelper swapWaiter = swapObserver.getCallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(() -> tab.addObserver(swapObserver));
        int currSwapCount = swapWaiter.getCallCount();

        int dragStartX = 30;
        int dragStartY = contentView.getHeight() / 2;
        int dragEndX = dragStartX;
        int dragEndY = 30;
        long downTime = System.currentTimeMillis();

        // Initial touch to trigger activation.
        TouchCommon.dragStart(activity, dragStartX, dragStartY, downTime);

        // Wait for first activation.
        swapWaiter.waitForCallback(currSwapCount, 1);

        LayoutAfterTabContentsSwappedObserver layoutObserver =
                new LayoutAfterTabContentsSwappedObserver(tab);
        CallbackHelper layoutWaiter = layoutObserver.getCallbackHelper();
        int currLayoutCount = layoutWaiter.getCallCount();

        // Wait for the first layout after second activation (reactivation of predecessor).
        layoutWaiter.waitForCallback(currLayoutCount, 1);

        // Continue and finish drag.
        TouchCommon.dragTo(activity, dragStartX, dragEndX, dragStartY, dragEndY, 100, downTime);
        TouchCommon.dragEnd(activity, dragEndX, dragEndY, downTime);

        WebContents contents = mActivityTestRule.getWebContents();
        Assert.assertTrue(Coordinates.createFor(contents).getScrollYPixInt() > 0);
    }

    private static class AuthHandlerCallbackHelper
            extends CallbackHelper implements Callback<ChromeHttpAuthHandler> {
        private ChromeHttpAuthHandler mAuthHandler;

        public ChromeHttpAuthHandler getAuthHandler() {
            return mAuthHandler;
        }

        @Override
        public void onResult(ChromeHttpAuthHandler authHandler) {
            mAuthHandler = authHandler;
            notifyCalled();
        }
    }

    /**
     * Tests that ChromeHttpAuthHandler is triggered within portals.
     *
     * This is the Android counterpart to PortalBrowserTest.HttpBasicAuthenticationInPortal.
     */
    @Test
    @MediumTest
    @Feature({"Portals"})
    // See https://crbug.com/1278223#c19. Remove this feature flag once
    // portals are migrated off the multiple webcontents architecture.
    @DisableFeatures({ChromeFeatureList.MESSAGES_FOR_ANDROID_INFRASTRUCTURE})
    public void testHttpBasicAuthenticationInPortal() throws Exception {
        mActivityTestRule.startMainActivityWithURL(
                mTestServer.getURL("/chrome/test/data/android/about.html"));
        Tab tab = mActivityTestRule.getActivity().getActivityTab();
        final WebContents contents = tab.getWebContents();
        Assert.assertNotNull(contents);

        // TODO(jbroman): It would be nicer to augment the underlying code to support waiting on
        // promises, rather than needing to use domAutomationController here.
        JavaScriptUtils.runJavascriptWithAsyncResult(contents,
                "new Promise((resolve, reject) => {\n"
                        + "  let portal = document.createElement('portal');\n"
                        + "  portal.src = '/chrome/test/data/android/about.html?2';\n"
                        + "  portal.onload = () => resolve(true);\n"
                        + "  document.body.appendChild(portal);\n"
                        + "}).then(() => domAutomationController.send(true));");

        final WebContents portalContents = TestThreadUtils.runOnUiThreadBlocking(() -> {
            List<? extends WebContents> innerContents = contents.getInnerWebContents();
            Assert.assertEquals(1, innerContents.size());
            return innerContents.get(0);
        });

        final AuthHandlerCallbackHelper helper = new AuthHandlerCallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> ChromeHttpAuthHandler.setTestCreationCallback(helper));
        try {
            JavaScriptUtils.executeJavaScript(
                    portalContents, "location.href = '/auth-basic?realm=Aperture'");
            helper.waitForFirst();
        } finally {
            TestThreadUtils.runOnUiThreadBlocking(
                    () -> ChromeHttpAuthHandler.setTestCreationCallback(null));
        }

        final ChromeHttpAuthHandler authHandler = helper.getAuthHandler();
        Assert.assertNotNull(authHandler);

        CriteriaHelper.pollUiThread(authHandler::isShowingAuthDialog);
        ThreadUtils.runOnUiThread(() -> authHandler.proceed("basicuser", "secret"));
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(portalContents.getTitle(), Matchers.is("basicuser/secret"));
        });
    }

    /**
     * Tests that content in a portal is considered a page visit in browser history upon activation.
     */
    @Test
    @MediumTest
    @Feature({"Portals"})
    public void testBrowserHistoryUpdatesOnActivation() throws Exception {
        String mainUrl = mTestServer.getURL(
                "/chrome/test/data/android/portals/portal-to-basic-content.html");
        String mainTitle = "Test Portal";
        String portalUrl =
                mTestServer.getURL("/chrome/test/data/android/portals/basic-content.html");
        String portalTitle = "Test Portal Content";

        mActivityTestRule.startMainActivityWithURL(mainUrl);
        Tab tab = mActivityTestRule.getActivity().getActivityTab();

        // Content loaded in a portal should not be considered a page visit by the
        // user.
        List<HistoryItem> history = getBrowsingHistory(tab);
        Assert.assertEquals(1, history.size());
        Assert.assertEquals(mainUrl, history.get(0).getUrl().getSpec());
        Assert.assertEquals(mainTitle, history.get(0).getTitle());

        executeScriptAndAwaitSwap(tab, "activatePortal();");

        // Now that the portal has activated, its contents are presented to the user
        // as a navigation in the tab, so this should be considered a page visit.
        history = getBrowsingHistory(tab);
        Assert.assertEquals(2, history.size());
        Assert.assertEquals(portalUrl, history.get(0).getUrl().getSpec());
        Assert.assertEquals(portalTitle, history.get(0).getTitle());
        Assert.assertEquals(mainUrl, history.get(1).getUrl().getSpec());
        Assert.assertEquals(mainTitle, history.get(1).getTitle());
    }

    interface NotificationPredicate {
        boolean test(StatusBarNotification notification);
    }

    private NotificationPredicate mMediaCaptureNotificationPred = notification
            -> TextUtils.equals(notification.getTag(), "MediaCaptureNotificationService");
    private NotificationPredicate mMediaPlaybackNotificationPred =
            notification -> notification.getId() == R.id.media_playback_notification;

    private void waitForNotification(NotificationPredicate pred) {
        waitForNotification(pred, CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL);
    }

    private void waitForNotification(NotificationPredicate pred, long maxTimeoutMs) {
        CriteriaHelper.pollInstrumentationThread(() -> {
            StatusBarNotification notifications[] =
                    ((NotificationManager) ContextUtils.getApplicationContext().getSystemService(
                             Context.NOTIFICATION_SERVICE))
                            .getActiveNotifications();
            for (StatusBarNotification statusBarNotification : notifications) {
                if (pred.test(statusBarNotification)) {
                    return true;
                }
            }
            return false;
        }, maxTimeoutMs, CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }

    private void waitForNoNotifications(NotificationPredicate pred) {
        CriteriaHelper.pollInstrumentationThread(() -> {
            StatusBarNotification notifications[] =
                    ((NotificationManager) ContextUtils.getApplicationContext().getSystemService(
                             Context.NOTIFICATION_SERVICE))
                            .getActiveNotifications();
            for (StatusBarNotification statusBarNotification : notifications) {
                if (pred.test(statusBarNotification)) {
                    return false;
                }
            }
            return true;
        });
    }

    @Test
    @LargeTest
    @Feature({"Portals"})
    @DisabledTest(message = "https://crbug.com/1174005")
    public void testMediaCaptureNotificationVisibleAfterAdoption() throws Exception {
        String mainUrl = mTestServer.getURL("/chrome/test/data/android/portals/media-capture.html");
        mActivityTestRule.startMainActivityWithURL(mainUrl);
        Tab tab = mActivityTestRule.getActivity().getActivityTab();

        // Start video capture.
        JavaScriptUtils.executeJavaScript(tab.getWebContents(), "start()");
        // Wait for permissions dialog.
        CriteriaHelper.pollUiThread(() -> {
            PermissionDialogController permissionDialogController =
                    PermissionDialogController.getInstance();
            return permissionDialogController.isDialogShownForTest();
        }, 12000, CriteriaHelper.DEFAULT_POLLING_INTERVAL);
        // Accept permissions request by clicking button on permissions dialog.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            PermissionDialogController permissionDialogController =
                    PermissionDialogController.getInstance();
            permissionDialogController.clickButtonForTest(
                    ModalDialogProperties.ButtonType.POSITIVE);
        });
        // Wait for video capture notification.
        waitForNotification(mMediaCaptureNotificationPred, 12000);
        // Activate portal.
        executeScriptAndAwaitSwap(tab, "activate()");
        // Wait for adoption to complete.
        JavaScriptUtils.runJavascriptWithAsyncResult(tab.getWebContents(), "waitForAdoption()");
        // Check if notification is still shown.
        waitForNotification(mMediaCaptureNotificationPred);
    }

    @Test
    @LargeTest
    @Feature({"Portals"})
    @DisabledTest(message = "https://crbug.com/1184291")
    public void testMediaNotificationDisappearsAfterActivation() throws Exception {
        String mainUrl =
                mTestServer.getURL("/chrome/test/data/android/portals/media-notification.html");
        mActivityTestRule.startMainActivityWithURL(mainUrl);
        Tab tab = mActivityTestRule.getActivity().getActivityTab();

        // Start video playback.
        DOMUtils.clickNode(tab.getWebContents(), "video");
        // Wait for media notification.
        waitForNotification(mMediaPlaybackNotificationPred);
        // Activate portal.
        executeScriptAndAwaitSwap(tab, "activate()");
        // Wait for adoption to complete.
        JavaScriptUtils.runJavascriptWithAsyncResult(tab.getWebContents(), "waitForAdoption()");
        // Notification should disappear.
        waitForNoNotifications(mMediaPlaybackNotificationPred);
        // Activate predecessor.
        executeScriptAndAwaitSwap(tab, "activate()");
        // Media notification should show again.
        waitForNotification(mMediaPlaybackNotificationPred);
    }

    @Test
    @LargeTest
    @Feature({"Portals", "AppBanners"})
    public void testAppBannerTriggerableAfterActivation() throws Exception {
        // Based on AppBannerManagerTest.
        mActivityTestRule.startMainActivityWithURL(
                mTestServer.getURL("/chrome/test/data/android/about.html"));
        final Tab tab = mActivityTestRule.getActivity().getActivityTab();
        final WebContents contents = tab.getWebContents();
        Assert.assertNotNull(contents);

        final String url = WebappTestPage.getServiceWorkerUrlWithAction(
                mTestServer, "call_stashed_prompt_on_click");
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            SiteEngagementService.getForBrowserContext(Profile.getLastUsedRegularProfile())
                    .resetBaseScoreForUrl(url, 10);
        });

        JavaScriptUtils.executeJavaScript(contents,
                "let portal = document.createElement('portal');\n"
                        + "portal.src = '" + url + "';\n"
                        + "portal.onload = () => portal.activate();\n"
                        + "document.body.appendChild(portal);");

        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(tab.getTitle(), Matchers.is("Web app banner test page"));
        });
        CriteriaHelper.pollUiThread(
                () -> !AppBannerManager.forWebContents(tab.getWebContents()).isRunningForTesting());
        TouchCommon.singleClickView(tab.getView());

        String expectedDialogTitle = ThreadUtils.runOnUiThreadBlocking(() -> {
            return mActivityTestRule.getActivity().getString(
                    AppBannerManager.getHomescreenLanguageOption(tab.getWebContents()).titleTextId);
        });
        UiObject dialogUiObject = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())
                                          .findObject(new UiSelector().text(expectedDialogTitle));
        Assert.assertTrue(dialogUiObject.waitForExists(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL));
    }

    @Test
    @MediumTest
    @Feature({"Portals"})
    public void testPermissionDeniedWhenRequestedByPortal() throws Exception {
        String mainUrl = mTestServer.getURL("/chrome/test/data/android/portals/geolocation.html");
        mActivityTestRule.startMainActivityWithURL(mainUrl);
        Tab tab = mActivityTestRule.getActivity().getActivityTab();
        final WebContents portalContents = TestThreadUtils.runOnUiThreadBlocking(() -> {
            List<? extends WebContents> innerContents = tab.getWebContents().getInnerWebContents();
            Assert.assertEquals(1, innerContents.size());
            return innerContents.get(0);
        });
        Assert.assertEquals("false",
                JavaScriptUtils.runJavascriptWithAsyncResult(portalContents, "requestLocation()"));
        Assert.assertEquals("\"denied\"",
                JavaScriptUtils.runJavascriptWithAsyncResult(
                        portalContents, "queryGeolocationPermission()"));
    }

    @Test
    @LargeTest
    @Feature({"Portals"})
    @DisabledTest(message = "https://crbug.com/1340918")
    public void testPermissionDeniedInPortalAfterBeingGrantedInPortalHost() throws Exception {
        String mainUrl = mTestServer.getURL("/chrome/test/data/android/portals/geolocation.html");
        mActivityTestRule.startMainActivityWithURL(mainUrl);
        Tab tab = mActivityTestRule.getActivity().getActivityTab();

        Assert.assertEquals("\"prompt\"",
                JavaScriptUtils.runJavascriptWithAsyncResult(
                        tab.getWebContents(), "queryGeolocationPermission()"));
        // Request geolocation.
        JavaScriptUtils.executeJavaScript(tab.getWebContents(), "requestLocation()");
        // Wait for permissions dialog.
        CriteriaHelper.pollUiThread(() -> {
            PermissionDialogController permissionDialogController =
                    PermissionDialogController.getInstance();
            return permissionDialogController.isDialogShownForTest();
        });
        // Accept permissions request by clicking button on permissions dialog.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            PermissionDialogController permissionDialogController =
                    PermissionDialogController.getInstance();
            permissionDialogController.clickButtonForTest(
                    ModalDialogProperties.ButtonType.POSITIVE);
        });
        // Assert that main page has geolocation permissions.
        Assert.assertEquals("\"granted\"",
                JavaScriptUtils.runJavascriptWithAsyncResult(
                        tab.getWebContents(), "queryGeolocationPermission()"));
        // Check portal for geolocation permission status and assert that it is denied.
        final WebContents portalContents = getPortalContents(tab);
        Assert.assertEquals("\"denied\"",
                JavaScriptUtils.runJavascriptWithAsyncResult(
                        portalContents, "queryGeolocationPermission()"));
    }

    @Test
    @MediumTest
    @Feature({"Portals"})
    public void testPermissionShouldBeDeniedAfterActivatingPortal() throws Exception {
        String mainUrl = mTestServer.getURL("/chrome/test/data/android/portals/geolocation.html");
        mActivityTestRule.startMainActivityWithURL(mainUrl);
        Tab tab = mActivityTestRule.getActivity().getActivityTab();
        WebContents webContents = tab.getWebContents();
        Assert.assertEquals("\"prompt\"",
                JavaScriptUtils.runJavascriptWithAsyncResult(
                        webContents, "queryGeolocationPermission()"));
        JavaScriptUtils.executeJavaScript(webContents, "requestLocation()");
        // Wait for permissions dialog.
        CriteriaHelper.pollUiThread(() -> {
            PermissionDialogController permissionDialogController =
                    PermissionDialogController.getInstance();
            return permissionDialogController.isDialogShownForTest();
        });
        // Activate portal while dialog is visible.
        executeScriptAndAwaitSwap(tab, "activatePortal()");
        JavaScriptUtils.runJavascriptWithAsyncResult(tab.getWebContents(), "waitForAdoption()");
        final WebContents portalContents = getPortalContents(tab);
        // Permission should be denied.
        Assert.assertEquals("false",
                JavaScriptUtils.runJavascriptWithAsyncResult(
                        portalContents, "waitForRequestLocationToResolve()"));
        // Permission dialog should not be visible anymore.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            PermissionDialogController permissionDialogController =
                    PermissionDialogController.getInstance();
            Assert.assertFalse(permissionDialogController.isDialogShownForTest());
        });
    }

    @Test
    @MediumTest
    @Feature({"Portals"})
    public void testPermissionDeniedWhenRequestedByAdoptedPortal() throws Exception {
        String mainUrl = mTestServer.getURL("/chrome/test/data/android/portals/geolocation.html");
        mActivityTestRule.startMainActivityWithURL(mainUrl);
        Tab tab = mActivityTestRule.getActivity().getActivityTab();
        // Activate portal and adopt predecessor.
        executeScriptAndAwaitSwap(tab, "activatePortal()");
        JavaScriptUtils.runJavascriptWithAsyncResult(tab.getWebContents(), "waitForAdoption()");
        final WebContents portalContents = getPortalContents(tab);
        // Request for geolocation should be denied.
        JavaScriptUtils.executeJavaScript(portalContents, "requestLocation()");
        Assert.assertEquals("false",
                JavaScriptUtils.runJavascriptWithAsyncResult(
                        portalContents, "waitForRequestLocationToResolve()"));
    }
}
