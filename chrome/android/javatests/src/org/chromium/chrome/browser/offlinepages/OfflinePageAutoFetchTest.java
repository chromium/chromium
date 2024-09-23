// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages;

import android.content.Intent;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestWatcher;
import org.junit.runner.Description;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RequiresRestart;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.device.DeviceConditions;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.net.NetworkChangeNotifier;
import org.chromium.net.test.util.WebServer;
import org.chromium.net.test.util.WebServer.HTTPRequest;
import org.chromium.ui.base.PageTransition;

import java.io.IOException;
import java.io.OutputStream;
import java.util.concurrent.Callable;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicReference;

/** Unit tests for auto-fetch-on-net-error-page. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class OfflinePageAutoFetchTest {
    private static final String TAG = "AutoFetchTest";
    private static final long WAIT_TIMEOUT_MS = 20000;

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public TestWatcher mTestWatcher =
            new TestWatcher() {
                @Override
                protected void failed(Throwable e, Description description) {
                    try {
                        logAdditionalContext();
                    } catch (Exception ex) {
                        // Exceptions here are typical if the test failed to start. Catch them, or
                        // it will obscure the actual failure.
                        Log.w(TAG, "Failed to log additional context: " + ex.toString());
                    }
                }
            };

    private Profile mProfile;
    private OfflinePageBridge mOfflinePageBridge;
    private CallbackHelper mPageAddedHelper = new CallbackHelper();
    private OfflinePageItem mAddedPage;
    private WebServer mWebServer;

    private Intent mLastInProgressCancelButtonIntent;
    private Intent mLastInProgressDeleteIntent;
    private Intent mLastCompleteClickIntent;
    private Intent mLastCompleteDeleteIntent;

    private class NotifierHooks implements AutoFetchNotifier.TestHooks {
        @Override
        public void inProgressNotificationShown(Intent cancelButtonIntent, Intent deleteIntent) {
            mLastInProgressCancelButtonIntent = cancelButtonIntent;
            mLastInProgressDeleteIntent = deleteIntent;
        }

        @Override
        public void completeNotificationShown(Intent clickIntent, Intent deleteIntent) {
            mLastCompleteClickIntent = clickIntent;
            mLastCompleteDeleteIntent = deleteIntent;
        }
    }

    private static final String DEFAULT_BODY = "<html><title>MyTestPage</title>Hello World!</html>";

    private void startWebServer() throws Exception {
        Assert.assertTrue(mWebServer == null);
        mWebServer = new WebServer(0, false);
        useDefaultWebServerResponse();
    }

    private void useDefaultWebServerResponse() {
        Assert.assertTrue(mWebServer != null);
        mWebServer.setRequestHandler(
                (HTTPRequest request, OutputStream stream) -> {
                    try {
                        WebServer.writeResponse(
                                stream, WebServer.STATUS_OK, DEFAULT_BODY.getBytes());
                    } catch (IOException e) {
                    }
                });
    }

    private void useAlternateWebServerResponse() {
        Assert.assertTrue(mWebServer != null);
        String body = "<html><title>A Different Page</title>Alternate page!</html>";
        mWebServer.setRequestHandler(
                (HTTPRequest request, OutputStream stream) -> {
                    try {
                        WebServer.writeResponse(stream, WebServer.STATUS_OK, body.getBytes());
                    } catch (IOException e) {
                    }
                });
    }

    private void useRedirectWebServerResponse() {
        Assert.assertTrue(mWebServer != null);
        String redirectBody =
                "<html><meta http-equiv=\"refresh\" content=\"0; url=/redirect_target\">"
                        + "<title>RedirectingFromHere</title>redirect</html>";
        mWebServer.setRequestHandler(
                (HTTPRequest request, OutputStream stream) -> {
                    try {
                        String body =
                                request.getURI().endsWith("redirect_from")
                                        ? redirectBody
                                        : DEFAULT_BODY;
                        WebServer.writeResponse(stream, WebServer.STATUS_OK, body.getBytes());
                    } catch (IOException e) {
                    }
                });
    }

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();

        AutoFetchNotifier.mTestHooks = new NotifierHooks();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mProfile = activityTab().getProfile();
                    mOfflinePageBridge = OfflinePageBridge.getForProfile(mProfile);

                    if (!NetworkChangeNotifier.isInitialized()) {
                        NetworkChangeNotifier.init();
                    }

                    OfflinePageBridge.getForProfile(mProfile)
                            .addObserver(
                                    new OfflinePageBridge.OfflinePageModelObserver() {
                                        @Override
                                        public void offlinePageAdded(OfflinePageItem addedPage) {
                                            mAddedPage = addedPage;
                                            mPageAddedHelper.notifyCalled();
                                        }
                                    });
                });
        forceConnectivityState(false);
    }

    @After
    public void tearDown() {
        OfflineTestUtil.clearIntercepts();
        if (mWebServer != null) {
            mWebServer.shutdown();
        }
    }

    @Test
    @MediumTest
    @Feature({"OfflineAutoFetch"})
    @DisabledTest(message = "https://crbug.com/1108684")
    public void testAutoFetchTriggersOnDNSErrorWhenOffline() {
        attemptLoadPage("http://does.not.resolve.com");
        waitForRequestCount(1);
    }

    @Test
    @MediumTest
    @Feature({"OfflineAutoFetch"})
    @RequiresRestart("crbug.com/344665757")
    public void testAutoFetchDoesNotTriggerOnDNSErrorWhenOnline() {
        forceConnectivityState(true);
        attemptLoadPage("http://does.not.resolve.com");
        waitForRequestCount(0);
    }

    @Test
    @MediumTest
    @Feature({"OfflineAutoFetch"})
    @DisabledTest(message = "https://crbug.com/1424463")
    public void testAutoFetchOnDinoPage() throws Exception {
        startWebServer();
        final String testUrl = mWebServer.getBaseUrl();

        // Make |testUrl| return an offline error and attempt to load the page.
        // This should trigger an auto-fetch request.
        OfflineTestUtil.interceptWithOfflineError(testUrl);
        attemptLoadPage(testUrl);
        waitForRequestCount(1);

        // Navigate away from the page, so that the auto-fetch request is allowed to complete,
        // and go back online.
        attemptLoadPage(UrlConstants.ABOUT_URL);
        OfflineTestUtil.clearIntercepts();
        forceConnectivityState(true);
        OfflineTestUtil.startRequestCoordinatorProcessing();

        // Wait for the background request to complete.
        waitForPageAdded();
        Assert.assertTrue(mAddedPage != null);

        // Simulate click on the complete notification, and ensure the offline page loads by
        // swapping out the live page contents.
        useAlternateWebServerResponse();
        sendBroadcast(mLastCompleteClickIntent);

        // A new tab should open, and it should load the offline page.
        pollInstrumentationThread(
                () -> {
                    return getCurrentTabModel().getCount() == 2
                            && ChromeTabUtils.getTitleOnUiThread(getCurrentTab())
                                    .equals("MyTestPage");
                });
    }

    @Test
    @MediumTest
    @Feature({"OfflineAutoFetch"})
    @DisabledTest(message = "https://crbug.com/1042215")
    public void testAutoFetchWithRedirect() throws Exception {
        startWebServer();
        useRedirectWebServerResponse();
        final String testUrl = mWebServer.getBaseUrl() + "/redirect_from";

        // Make |testUrl| return an offline error and attempt to load the page.
        // This should trigger an auto-fetch request.
        OfflineTestUtil.interceptWithOfflineError(testUrl);
        attemptLoadPage(testUrl);
        waitForRequestCount(1);

        // Navigate away from the page, so that the auto-fetch request is allowed to complete,
        // and go back online.
        attemptLoadPage(UrlConstants.ABOUT_URL);
        OfflineTestUtil.clearIntercepts();
        forceConnectivityState(true);
        OfflineTestUtil.startRequestCoordinatorProcessing();

        // Wait for the background request to complete.
        waitForPageAdded();
        Assert.assertTrue(mAddedPage != null);

        // Navigate back to testUrl, this time there is no redirect.
        useDefaultWebServerResponse();
        attemptLoadPage(testUrl);

        // Simulate click on the complete notification, and ensure the offline page loads by
        // swapping out the live page contents.
        useAlternateWebServerResponse();
        sendBroadcast(mLastCompleteClickIntent);

        pollInstrumentationThread(
                () -> {
                    // No new tab is opened, because the URL of the tab matches the original URL.
                    return getCurrentTabModel().getCount() == 1
                            // The title matches the original page, not the
                            // 'AlternativeWebServerResponse'.
                            && ChromeTabUtils.getTitleOnUiThread(getCurrentTab())
                                    .equals("MyTestPage");
                });
    }

    @Test
    @MediumTest
    @Feature({"OfflineAutoFetch"})
    @DisabledTest(message = "https://crbug.com/1424463")
    public void testSwipeAwayCompleteNotification() throws Exception {
        // Standard setup to trigger auto-fetch.
        startWebServer();
        final String testUrl = mWebServer.getBaseUrl();
        OfflineTestUtil.interceptWithOfflineError(testUrl);
        attemptLoadPage(testUrl);
        waitForRequestCount(1);
        attemptLoadPage(UrlConstants.ABOUT_URL);
        OfflineTestUtil.clearIntercepts();
        forceConnectivityState(true);
        OfflineTestUtil.startRequestCoordinatorProcessing();

        // Wait for the background request to complete.
        waitForPageAdded();

        // Simulate swiping away the complete notification and wait for UMA change.
        sendBroadcast(mLastCompleteDeleteIntent);
    }

    @Test
    @MediumTest
    @Feature({"OfflineAutoFetch"})
    public void testAutoFetchCancelOnLoad() throws Exception {
        startWebServer();
        final String testUrl = mWebServer.getBaseUrl();
        // Make |testUrl| return an offline error and attempt to load the page.
        // This should trigger an auto-fetch request.
        OfflineTestUtil.interceptWithOfflineError(testUrl);
        attemptLoadPage(testUrl);
        waitForRequestCount(1);

        // Allow loading the page and try again.
        OfflineTestUtil.clearIntercepts();
        OfflineTestUtil.startRequestCoordinatorProcessing();
        mActivityTestRule.loadUrl(testUrl);

        // |testUrl| should successfully load and the auto-fetch request should be removed.
        waitForRequestCount(0);
        Assert.assertEquals(0, mPageAddedHelper.getCallCount());
    }

    @Test
    @MediumTest
    @Feature({"OfflineAutoFetch"})
    @DisabledTest(message = "https://crbug.com/923212")
    public void testAutoFetchRequestRetainedOnOtherTabClosed() throws Exception {
        startWebServer();
        final String testUrl = mWebServer.getBaseUrl();
        // Make |testUrl| return an offline error and attempt to load the page.
        // This should trigger an auto-fetch request.
        OfflineTestUtil.interceptWithOfflineError(testUrl);
        attemptLoadPage(testUrl);
        waitForRequestCount(1);

        // Attempt to load the same URL in a new tab, and then close the tab.
        // This should not create a new request.
        Tab newTab = attemptLoadPageInNewTab(testUrl);
        closeTab(newTab);
        Assert.assertEquals(1, OfflineTestUtil.getRequestsInQueue().length);

        // The original request should remain. Allow the request to complete.
        closeTab(activityTab());
        OfflineTestUtil.clearIntercepts();
        forceConnectivityState(true);
        OfflineTestUtil.startRequestCoordinatorProcessing();
        waitForPageAdded();
    }

    @Test
    @MediumTest
    @Feature({"OfflineAutoFetch"})
    public void testAutoFetchNotifyOnTabClose() throws Exception {
        final String testUrl = "http://www.offline.com";
        // Make |testUrl| return an offline error and attempt to load the page.
        // This should trigger an auto-fetch request.
        OfflineTestUtil.interceptWithOfflineError(testUrl);
        attemptLoadPage(testUrl);
        waitForRequestCount(1);

        closeTab(activityTab());
    }

    @Test
    @MediumTest
    @Feature({"OfflineAutoFetch"})
    @DisabledTest(message = "https://crbug.com/1424463")
    public void testAutoFetchSwipeInProgressNotification() throws Exception {
        // Trigger an auto-fetch request, and then an in-progress notification.
        final String testUrl = "http://www.offline.com";
        OfflineTestUtil.interceptWithOfflineError(testUrl);
        attemptLoadPage(testUrl);
        waitForRequestCount(1);
        closeTab(activityTab());

        // Simulate swiping the notification by sending the delete intent. This should trigger
        // deletion of the request.
        sendBroadcast(mLastInProgressDeleteIntent);
        waitForRequestCount(0);
    }

    @Test
    @MediumTest
    @Feature({"OfflineAutoFetch"})
    @DisabledTest(message = "https://crbug.com/1426451, https://crbug.com/1424463")
    public void testAutoFetchTwoRequestsCancel() throws Exception {
        // Trigger two auto-fetch requests.
        final String testUrl1 = "http://www.offline1.com";
        OfflineTestUtil.interceptWithOfflineError(testUrl1);
        attemptLoadPage(testUrl1);
        waitForRequestCount(1);
        OfflineTestUtil.clearIntercepts(); // Only one intercept works at a time.

        final String testUrl2 = "http://www.offline2.com";
        OfflineTestUtil.interceptWithOfflineError(testUrl2);
        attemptLoadPage(testUrl2);
        waitForRequestCount(2);

        // Trigger the in-progress notification, and then simulate tapping 'cancel'. Note that the
        // in-progress notification is triggered for both requests, but only fires a single
        // notification.
        closeTab(activityTab());
        sendBroadcast(mLastInProgressCancelButtonIntent);

        waitForRequestCount(0);
        // Ensure the cancellation preference is cleared.
        Assert.assertEquals(false, AutoFetchNotifier.autoFetchInProgressNotificationCanceled());
    }

    private void waitForRequestCount(int requestCount) {
        pollInstrumentationThread(
                () -> OfflineTestUtil.getRequestsInQueue().length == requestCount);
    }

    private void waitForPageAdded() throws Exception {
        mPageAddedHelper.waitForCallback(0, 1, WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS);
    }

    private Tab activityTab() {
        return mActivityTestRule.getActivity().getActivityTab();
    }

    // Attempt to load a page on the active tab. Does not assert that the page is loaded
    // successfully.
    private void attemptLoadPage(String url) {
        Tab tab = activityTab();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    tab.loadUrl(
                            new LoadUrlParams(
                                    url, PageTransition.TYPED | PageTransition.FROM_ADDRESS_BAR));
                });
    }

    // Attempts to create a new tab and load |url| in it.
    private Tab attemptLoadPageInNewTab(String url) throws Exception {
        ChromeActivity activity = mActivityTestRule.getActivity();
        Tab tab =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                activity.getTabCreator(false)
                                        .launchUrl(url, TabLaunchType.FROM_LINK));
        ChromeTabUtils.waitForInteractable(tab);
        return tab;
    }

    private boolean isErrorPage(final Tab tab) {
        final AtomicReference<Boolean> result = new AtomicReference<Boolean>(false);
        ThreadUtils.runOnUiThreadBlocking(() -> result.set(tab.isShowingErrorPage()));
        return result.get();
    }

    private void closeTab(Tab tab) {
        final TabModel model =
                mActivityTestRule.getActivity().getTabModelSelector().getCurrentModel();

        // Attempt to close the tab, which will delay closing until the undo timeout goes away.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabModelUtils.closeTabById(model, tab.getId(), true);
                });
    }

    private void forceConnectivityState(boolean connected) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    NetworkChangeNotifier.forceConnectivityState(connected);
                    DeviceConditions.sForceConnectionTypeForTesting = !connected;
                });
        OfflineTestUtil.waitForConnectivityState(connected);
    }

    private void sendBroadcast(Intent intent) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ContextUtils.getApplicationContext().sendBroadcast(intent);
                });
    }

    private TabModel getCurrentTabModel() {
        return mActivityTestRule.getActivity().getCurrentTabModel();
    }

    private Tab getCurrentTab() {
        return TabModelUtils.getCurrentTab(getCurrentTabModel());
    }

    private void logAdditionalContext() {
        TabModel tabModel = getCurrentTabModel();
        // Return early if the test setup didn't complete.
        if (tabModel == null) {
            return;
        }
        Log.d(TAG, "Logging additional context");
        int tabCount = tabModel.getCount();
        Log.d(TAG, "Tab Count: " + tabCount);
        for (int i = 0; i < tabCount; ++i) {
            String title = ChromeTabUtils.getTitleOnUiThread(tabModel.getTabAt(i));
            String current = tabModel.index() == i ? "*current" : "";
            Log.d(TAG, "Tab " + String.valueOf(i) + " '" + title + "' " + current);
        }
        try {
            Log.d(
                    TAG,
                    "Request Coordinator state:" + OfflineTestUtil.dumpRequestCoordinatorState());
        } catch (TimeoutException e) {
        }
    }

    private void pollInstrumentationThread(final Callable<Boolean> criteria) {
        CriteriaHelper.pollInstrumentationThread(
                criteria, "Criteria not met", WAIT_TIMEOUT_MS, 100);
    }
}
