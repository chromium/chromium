// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.hamcrest.Matchers.is;
import static org.junit.Assert.assertEquals;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;
import static org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule.LONG_TIMEOUT_MS;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;

import androidx.browser.customtabs.CustomTabsCallback;
import androidx.browser.customtabs.CustomTabsIntent;
import androidx.browser.customtabs.CustomTabsService;
import androidx.browser.customtabs.CustomTabsSession;
import androidx.browser.customtabs.CustomTabsSessionToken;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.TerminationStatus;
import org.chromium.base.ThreadUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.content_public.browser.test.util.WebContentsUtils;
import org.chromium.net.test.util.TestWebServer;

/** Integration tests for the Custom Tab post message support. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class CustomTabPostMessageTest {
    private static final int BEFORE_MAY_LAUNCH_URL = 0;
    private static final int BEFORE_INTENT = 1;
    private static final int AFTER_INTENT = 2;

    private static final String JS_MESSAGE = "from_js";
    private static final String TEST_PAGE = "/chrome/test/data/android/google.html";
    private static final String TEST_PAGE_2 = "/chrome/test/data/android/test.html";

    private static final String TITLE_FROM_POSTMESSAGE_TO_CHANNEL =
            "<!DOCTYPE html><html><body>"
                    + "    <script>"
                    + "        var received = '';"
                    + "        onmessage = function (e) {"
                    + "            var myport = e.ports[0];"
                    + "            myport.onmessage = function (f) {"
                    + "                received += f.data;"
                    + "                document.title = received;"
                    + "            }"
                    + "        }"
                    + "   </script>"
                    + "</body></html>";
    private static final String MESSAGE_FROM_PAGE_TO_CHANNEL =
            "<!DOCTYPE html><html><body>"
                    + "    <script>"
                    + "        onmessage = function (e) {"
                    + "            if (e.ports != null && e.ports.length > 0) {"
                    + "               e.ports[0].postMessage(\""
                    + JS_MESSAGE
                    + "\");"
                    + "            }"
                    + "        }"
                    + "   </script>"
                    + "</body></html>";

    @Rule
    public CustomTabActivityTestRule mCustomTabActivityTestRule = new CustomTabActivityTestRule();

    private String mTestPage;
    private String mTestPage2;
    private CustomTabsConnection mConnectionToCleanup;
    private TestWebServer mWebServer;

    @Before
    public void setUp() throws Exception {
        mTestPage = mCustomTabActivityTestRule.getTestServer().getURL(TEST_PAGE);
        mTestPage2 = mCustomTabActivityTestRule.getTestServer().getURL(TEST_PAGE_2);
        LibraryLoader.getInstance().ensureInitialized();
        mWebServer = TestWebServer.start();
    }

    @After
    public void tearDown() {
        if (mConnectionToCleanup != null) CustomTabsTestUtils.cleanupSessions(mConnectionToCleanup);
        if (mWebServer != null) mWebServer.shutdown();
    }

    private void waitForTitle(String newTitle) {
        Tab currentTab = mCustomTabActivityTestRule.getActivity().getActivityTab();
        ChromeTabUtils.waitForTitle(currentTab, newTitle);
    }

    private void setCanUseHiddenTabForSession(
            CustomTabsConnection connection, CustomTabsSessionToken token, boolean useHiddenTab) {
        assert mConnectionToCleanup == null || mConnectionToCleanup == connection;
        // Save the connection. In case the hidden tab is not consumed by the test, ensure that it
        // is properly cleaned up after the test.
        mConnectionToCleanup = connection;
        connection.setCanUseHiddenTabForSession(token, useHiddenTab);
    }

    private static void ensureCompletedSpeculationForUrl(
            final CustomTabsConnection connection, final String url) {
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            "Tab was not created",
                            connection.getSpeculationParamsForTesting(),
                            Matchers.notNullValue());
                },
                LONG_TIMEOUT_MS,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
        ChromeTabUtils.waitForTabPageLoaded(connection.getSpeculationParamsForTesting().tab, url);
    }

    /**
     * Tests that basic postMessage functionality works through sending a single postMessage
     * request.
     */
    @Test
    @SmallTest
    public void testPostMessageBasic() throws Exception {
        final CustomTabsConnection connection = CustomTabsTestUtils.warmUpAndWait();
        Context context = ApplicationProvider.getApplicationContext();
        Intent intent = CustomTabsIntentTestUtils.createMinimalCustomTabIntent(context, mTestPage);
        final CustomTabsSessionToken token =
                CustomTabsSessionToken.getSessionTokenFromIntent(intent);
        Assert.assertTrue(connection.newSession(token));
        Assert.assertTrue(connection.requestPostMessageChannel(token, null, null));
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    final Tab currentTab =
                            mCustomTabActivityTestRule.getActivity().getActivityTab();
                    Criteria.checkThat(
                            ChromeTabUtils.getUrlStringOnUiThread(currentTab), is(mTestPage));
                });
        Assert.assertTrue(
                connection.postMessage(token, "Message", null) == CustomTabsService.RESULT_SUCCESS);
        ThreadUtils.runOnUiThreadBlocking(
                (Runnable)
                        () ->
                                mCustomTabActivityTestRule
                                        .getActivity()
                                        .getActivityTab()
                                        .loadUrl(new LoadUrlParams(mTestPage2)));
        CriteriaHelper.pollUiThread(
                () -> {
                    final Tab currentTab =
                            mCustomTabActivityTestRule.getActivity().getActivityTab();
                    return ChromeTabUtils.isLoadingAndRenderingDone(currentTab);
                });
        Assert.assertTrue(
                connection.postMessage(token, "Message", null)
                        == CustomTabsService.RESULT_FAILURE_MESSAGING_ERROR);
    }

    /**
     * Tests that postMessage channel is not functioning after web contents get destroyed and also
     * not breaking things.
     */
    @Test
    @SmallTest
    public void testPostMessageWebContentsDestroyed() throws Exception {
        final CustomTabsConnection connection = CustomTabsTestUtils.warmUpAndWait();
        Context context = ApplicationProvider.getApplicationContext();
        Intent intent = CustomTabsIntentTestUtils.createMinimalCustomTabIntent(context, mTestPage);
        final CustomTabsSessionToken token =
                CustomTabsSessionToken.getSessionTokenFromIntent(intent);
        Assert.assertTrue(connection.newSession(token));
        Assert.assertTrue(connection.requestPostMessageChannel(token, null, null));
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    final Tab currentTab =
                            mCustomTabActivityTestRule.getActivity().getActivityTab();
                    Criteria.checkThat(
                            ChromeTabUtils.getUrlStringOnUiThread(currentTab), is(mTestPage));
                });
        Assert.assertTrue(
                connection.postMessage(token, "Message", null) == CustomTabsService.RESULT_SUCCESS);

        final CallbackHelper renderProcessCallback = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    new WebContentsObserver(mCustomTabActivityTestRule.getWebContents()) {
                        @Override
                        public void primaryMainFrameRenderProcessGone(
                                @TerminationStatus int terminationStatus) {
                            renderProcessCallback.notifyCalled();
                        }
                    };
                });
        PostTask.postTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    WebContentsUtils.simulateRendererKilled(
                            mCustomTabActivityTestRule
                                    .getActivity()
                                    .getActivityTab()
                                    .getWebContents());
                });
        renderProcessCallback.waitForCallback(0);
        Assert.assertTrue(
                connection.postMessage(token, "Message", null)
                        == CustomTabsService.RESULT_FAILURE_MESSAGING_ERROR);
    }

    /**
     * Tests whether validatePostMessageOrigin is necessary for making successful postMessage
     * requests.
     */
    @Test
    @SmallTest
    public void testPostMessageRequiresValidation() throws Exception {
        final CustomTabsConnection connection = CustomTabsTestUtils.warmUpAndWait();
        Context context = ApplicationProvider.getApplicationContext();
        Intent intent = CustomTabsIntentTestUtils.createMinimalCustomTabIntent(context, mTestPage);
        final CustomTabsSessionToken token =
                CustomTabsSessionToken.getSessionTokenFromIntent(intent);
        Assert.assertTrue(connection.newSession(token));
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    final Tab currentTab =
                            mCustomTabActivityTestRule.getActivity().getActivityTab();
                    Criteria.checkThat(
                            ChromeTabUtils.getUrlStringOnUiThread(currentTab), is(mTestPage));
                });
        Assert.assertTrue(
                connection.postMessage(token, "Message", null)
                        == CustomTabsService.RESULT_FAILURE_MESSAGING_ERROR);
    }

    /**
     * Tests the sent postMessage requests not only return success, but is also received by page.
     */
    @Test
    @SmallTest
    public void testPostMessageReceivedInPage() throws Exception {
        final String url =
                mWebServer.setResponse("/test.html", TITLE_FROM_POSTMESSAGE_TO_CHANNEL, null);
        final CustomTabsConnection connection = CustomTabsTestUtils.warmUpAndWait();
        Context context = ApplicationProvider.getApplicationContext();
        Intent intent = CustomTabsIntentTestUtils.createMinimalCustomTabIntent(context, url);
        final CustomTabsSessionToken token =
                CustomTabsSessionToken.getSessionTokenFromIntent(intent);
        Assert.assertTrue(connection.newSession(token));
        Assert.assertTrue(connection.requestPostMessageChannel(token, null, null));
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    final Tab currentTab =
                            mCustomTabActivityTestRule.getActivity().getActivityTab();
                    Criteria.checkThat(ChromeTabUtils.getUrlStringOnUiThread(currentTab), is(url));
                });
        Assert.assertTrue(
                connection.postMessage(token, "New title", null)
                        == CustomTabsService.RESULT_SUCCESS);
        waitForTitle("New title");
    }

    /** Tests the postMessage requests sent from the page is received on the client side. */
    @Test
    @SmallTest
    public void testPostMessageReceivedFromPage() throws Exception {
        final CallbackHelper messageChannelHelper = new CallbackHelper();
        final CallbackHelper onPostMessageHelper = new CallbackHelper();
        final String url = mWebServer.setResponse("/test.html", MESSAGE_FROM_PAGE_TO_CHANNEL, null);
        CustomTabsTestUtils.warmUpAndWait();
        final CustomTabsSession session =
                CustomTabsTestUtils.bindWithCallback(
                                new CustomTabsCallback() {
                                    @Override
                                    public void onMessageChannelReady(Bundle extras) {
                                        messageChannelHelper.notifyCalled();
                                    }

                                    @Override
                                    public void onPostMessage(String message, Bundle extras) {
                                        onPostMessageHelper.notifyCalled();
                                    }
                                })
                        .session;
        session.requestPostMessageChannel(Uri.parse("https://www.example.com/"));
        Intent intent = new CustomTabsIntent.Builder(session).build().intent;
        intent.setData(Uri.parse(url));
        intent.setComponent(
                new ComponentName(
                        ApplicationProvider.getApplicationContext(), ChromeLauncherActivity.class));
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        Assert.assertTrue(
                session.postMessage("Message", null)
                        == CustomTabsService.RESULT_FAILURE_MESSAGING_ERROR);

        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
        messageChannelHelper.waitForCallback(0);
        onPostMessageHelper.waitForCallback(0);
    }

    /**
     * Tests the postMessage requests sent from the page is received on the client side even though
     * the request is sent after the page is created.
     */
    @Test
    @SmallTest
    @DisabledTest(message = "https://crbug.com/692025")
    public void testPostMessageReceivedFromPageWithLateRequest() throws Exception {
        final CallbackHelper messageChannelHelper = new CallbackHelper();
        final CallbackHelper onPostMessageHelper = new CallbackHelper();
        final String url = mWebServer.setResponse("/test.html", MESSAGE_FROM_PAGE_TO_CHANNEL, null);
        CustomTabsTestUtils.warmUpAndWait();
        final CustomTabsSession session =
                CustomTabsTestUtils.bindWithCallback(
                                new CustomTabsCallback() {
                                    @Override
                                    public void onMessageChannelReady(Bundle extras) {
                                        messageChannelHelper.notifyCalled();
                                    }

                                    @Override
                                    public void onPostMessage(String message, Bundle extras) {
                                        onPostMessageHelper.notifyCalled();
                                    }
                                })
                        .session;

        Intent intent = new CustomTabsIntent.Builder(session).build().intent;
        intent.setData(Uri.parse(url));
        intent.setComponent(
                new ComponentName(
                        ApplicationProvider.getApplicationContext(), ChromeLauncherActivity.class));
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    final Tab currentTab =
                            mCustomTabActivityTestRule.getActivity().getActivityTab();
                    Criteria.checkThat(ChromeTabUtils.getUrlStringOnUiThread(currentTab), is(url));
                });

        session.requestPostMessageChannel(Uri.parse("https://www.example.com/"));

        messageChannelHelper.waitForCallback(0);
        onPostMessageHelper.waitForCallback(0);

        Assert.assertTrue(session.postMessage("Message", null) == CustomTabsService.RESULT_SUCCESS);
    }

    /**
     * Tests a postMessage request chain can start while loading a hidden tab and continue
     * afterwards. Request sent before the hidden tab start.
     */
    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testPostMessageThroughHiddenTabWithRequestBeforeMayLaunchUrl() throws Exception {
        sendPostMessageDuringHiddenTabTransition(BEFORE_MAY_LAUNCH_URL);
    }

    /**
     * Tests a postMessage request chain can start while loading a hidden tab and continue
     * afterwards. Request sent after the hidden tab start and before intent launched.
     */
    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testPostMessageThroughHiddenTabWithRequestBeforeIntent() throws Exception {
        sendPostMessageDuringHiddenTabTransition(BEFORE_INTENT);
    }

    /**
     * Tests a postMessage request chain can start while loading a hidden tab and continue
     * afterwards. Request sent after intent received.
     */
    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testPostMessageThroughHiddenTabWithRequestAfterIntent() throws Exception {
        sendPostMessageDuringHiddenTabTransition(AFTER_INTENT);
    }

    private void sendPostMessageDuringHiddenTabTransition(int requestTime) throws Exception {
        final CallbackHelper messageChannelHelper = new CallbackHelper();
        final String url =
                mWebServer.setResponse("/test.html", TITLE_FROM_POSTMESSAGE_TO_CHANNEL, null);
        final CustomTabsConnection connection = CustomTabsTestUtils.warmUpAndWait();

        final CustomTabsSession session =
                CustomTabsTestUtils.bindWithCallback(
                                new CustomTabsCallback() {
                                    @Override
                                    public void onMessageChannelReady(Bundle extras) {
                                        messageChannelHelper.notifyCalled();
                                    }
                                })
                        .session;

        Intent intent = new CustomTabsIntent.Builder(session).build().intent;
        intent.setData(Uri.parse(url));
        intent.setComponent(
                new ComponentName(
                        ApplicationProvider.getApplicationContext(), ChromeLauncherActivity.class));
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        CustomTabsSessionToken token = CustomTabsSessionToken.getSessionTokenFromIntent(intent);

        boolean channelRequested = false;
        String titleString = "";

        if (requestTime == BEFORE_MAY_LAUNCH_URL) {
            channelRequested =
                    session.requestPostMessageChannel(Uri.parse("https://www.example.com/"));
            Assert.assertTrue(channelRequested);
        }

        setCanUseHiddenTabForSession(connection, token, true);
        session.mayLaunchUrl(Uri.parse(url), null, null);
        ensureCompletedSpeculationForUrl(connection, url);

        if (requestTime == BEFORE_INTENT) {
            channelRequested =
                    session.requestPostMessageChannel(Uri.parse("https://www.example.com/"));
            Assert.assertTrue(channelRequested);
        }

        if (channelRequested) {
            messageChannelHelper.waitForCallback(0);
            String currentMessage = "Prerendering ";
            // Initial title update during prerender.
            assertEquals(
                    CustomTabsService.RESULT_SUCCESS, session.postMessage(currentMessage, null));
            titleString = currentMessage;
        }

        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    final Tab currentTab =
                            mCustomTabActivityTestRule.getActivity().getActivityTab();
                    Criteria.checkThat(ChromeTabUtils.getUrlStringOnUiThread(currentTab), is(url));
                });

        if (requestTime == AFTER_INTENT) {
            channelRequested =
                    session.requestPostMessageChannel(Uri.parse("https://www.example.com/"));
            Assert.assertTrue(channelRequested);
            messageChannelHelper.waitForCallback(0);
        }

        String currentMessage = "and loading ";
        // Update title again and verify both updates went through with the channel still intact.
        assertEquals(CustomTabsService.RESULT_SUCCESS, session.postMessage(currentMessage, null));
        titleString += currentMessage;

        // Request a new channel, verify it was created.
        session.requestPostMessageChannel(Uri.parse("https://www.example.com/"));
        messageChannelHelper.waitForCallback(1);

        String newMessage = "and refreshing";
        // Update title again and verify both updates went through with the channel still intact.
        assertEquals(CustomTabsService.RESULT_SUCCESS, session.postMessage(newMessage, null));
        titleString += newMessage;

        final String title = titleString;
        waitForTitle(title);
    }
}
