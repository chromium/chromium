// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.router;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;

import android.app.Dialog;
import android.os.StrictMode;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.LargeTest;
import android.view.View;

import org.json.JSONObject;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.media.RouterTestUtils;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.ClickUtils;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.test.util.UiRestriction;

import java.io.StringWriter;

/**
 * Integration tests for MediaRouter.
 *
 * TODO(jbudorick): Remove this when media_router_integration_browsertest runs on Android.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ContentSwitches.DISABLE_GESTURE_REQUIREMENT_FOR_PRESENTATION,
        ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class MediaRouterIntegrationTest {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    private static final String TEST_PAGE =
            "/chrome/test/media_router/resources/basic_test.html?__is_android__=true";
    private static final String TEST_PAGE_RECONNECT_FAIL =
            "/chrome/test/media_router/resources/fail_reconnect_session.html?__is_android__=true";

    private static final String TEST_SINK_NAME = "test-sink-1";
    // The javascript snippets.
    private static final String UNSET_RESULT_SCRIPT = "lastExecutionResult = null";
    private static final String GET_RESULT_SCRIPT = "lastExecutionResult";
    private static final String CHECK_SESSION_SCRIPT = "checkSession();";
    private static final String CHECK_START_FAILED_SCRIPT = "checkStartFailed('%s', '%s');";
    private static final String START_SESSION_SCRIPT = "startSession();";
    private static final String TERMINATE_SESSION_SCRIPT =
            "terminateSessionAndWaitForStateChange();";
    private static final String WAIT_DEVICE_SCRIPT = "waitUntilDeviceAvailable();";
    private static final String SEND_MESSAGE_AND_EXPECT_RESPONSE_SCRIPT =
            "sendMessageAndExpectResponse('%s');";
    private static final String SEND_MESSAGE_AND_EXPECT_CONNECTION_CLOSE_ON_ERROR_SCRIPT =
            "sendMessageAndExpectConnectionCloseOnError()";

    private static final int VIEW_TIMEOUT_MS = 2000;
    private static final int VIEW_RETRY_MS = 100;
    private static final int SCRIPT_TIMEOUT_MS = 10000;
    private static final int SCRIPT_RETRY_MS = 50;

    private StrictMode.ThreadPolicy mOldPolicy;

    private EmbeddedTestServer mTestServer;

    @Before
    public void setUp() throws Exception {
        ChromeMediaRouter.setRouteProviderFactoryForTest(new MockMediaRouteProvider.Factory());
        mActivityTestRule.startMainActivityOnBlankPage();
        // Temporary until support library is updated, see http://crbug.com/576393.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mOldPolicy = StrictMode.allowThreadDiskWrites(); });
        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
    }

    @After
    public void tearDown() {
        // Temporary until support library is updated, see http://crbug.com/576393.
        TestThreadUtils.runOnUiThreadBlocking(() -> { StrictMode.setThreadPolicy(mOldPolicy); });
        mTestServer.stopAndDestroyServer();
    }

    // TODO(zqzhang): Move this to a util class?
    // TODO(zqzhang): This method does not handle Unicode escape sequences.
    private String unescapeString(String str) {
        assert str.charAt(0) == '\"' && str.charAt(str.length() - 1) == '\"';
        str = str.substring(1, str.length() - 1);
        StringWriter writer = new StringWriter();
        for (int i = 0; i < str.length(); i++) {
            char thisChar = str.charAt(i);
            if (thisChar != '\\') {
                writer.write(str.charAt(i));
                continue;
            }
            char nextChar = str.charAt(++i);
            switch (nextChar) {
                case 't':
                    writer.write('\t');
                    break;
                case 'b':
                    writer.write('\b');
                    break;
                case 'n':
                    writer.write('\n');
                    break;
                case 'r':
                    writer.write('\r');
                    break;
                case 'f':
                    writer.write('\f');
                    break;
                case '\'':
                    writer.write('\'');
                    break;
                case '\"':
                    writer.write('\"');
                    break;
                case '\\':
                    writer.write('\\');
                    break;
                default:
                    writer.write(nextChar);
            }
        }
        return writer.toString();
    }

    private void executeJavaScriptApi(WebContents webContents, String script) {
        executeJavaScriptApi(webContents, script, SCRIPT_TIMEOUT_MS, SCRIPT_RETRY_MS);
    }

    private void executeJavaScriptApi(
            final WebContents webContents, final String script, int maxTimeoutMs, int intervalMs) {
        try {
            JavaScriptUtils.executeJavaScriptAndWaitForResult(webContents, UNSET_RESULT_SCRIPT);
            JavaScriptUtils.executeJavaScriptAndWaitForResult(webContents, script);
            CriteriaHelper.pollInstrumentationThread(new Criteria() {
                @Override
                public boolean isSatisfied() {
                    try {
                        String result = JavaScriptUtils.executeJavaScriptAndWaitForResult(
                                webContents, GET_RESULT_SCRIPT);
                        return !result.equals("null");
                    } catch (Exception e) {
                        return false;
                    }
                }
            }, maxTimeoutMs, intervalMs);
            String unescapedResult =
                    unescapeString(JavaScriptUtils.executeJavaScriptAndWaitForResult(
                            webContents, GET_RESULT_SCRIPT));
            JSONObject jsonResult = new JSONObject(unescapedResult);
            Assert.assertTrue(
                    jsonResult.getString("errorMessage"), jsonResult.getBoolean("passed"));
        } catch (Exception e) {
            e.printStackTrace();
            Assert.fail("caught exception while executing javascript:" + script);
        }
    }

    String getJavaScriptVariable(WebContents webContents, String script) {
        try {
            String result = JavaScriptUtils.executeJavaScriptAndWaitForResult(webContents, script);
            if (result.charAt(0) == '\"' && result.charAt(result.length() - 1) == '\"') {
                result = result.substring(1, result.length() - 1);
            }
            return result;
        } catch (Exception e) {
            e.printStackTrace();
            Assert.fail();
            return null;
        }
    }

    void checkStartFailed(WebContents webContents, String errorName, String errorMessageSubstring) {
        String script = String.format(CHECK_START_FAILED_SCRIPT, errorName, errorMessageSubstring);
        executeJavaScriptApi(webContents, script);
    }

    @Test
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    @Feature({"MediaRouter"})
    @LargeTest
    @RetryOnFailure
    public void testBasic() {
        mActivityTestRule.loadUrl(mTestServer.getURL(TEST_PAGE));
        WebContents webContents = mActivityTestRule.getWebContents();
        executeJavaScriptApi(webContents, WAIT_DEVICE_SCRIPT);
        executeJavaScriptApi(webContents, START_SESSION_SCRIPT);
        View testRouteButton = RouterTestUtils.waitForRouteButton(
                mActivityTestRule.getActivity(), TEST_SINK_NAME, VIEW_TIMEOUT_MS, VIEW_RETRY_MS);
        ClickUtils.mouseSingleClickView(
                InstrumentationRegistry.getInstrumentation(), testRouteButton);
        executeJavaScriptApi(webContents, CHECK_SESSION_SCRIPT);
        String sessionId = getJavaScriptVariable(webContents, "startedConnection.id");
        Assert.assertFalse(sessionId.length() == 0);
        String defaultRequestSessionId =
                getJavaScriptVariable(webContents, "defaultRequestSessionId");
        Assert.assertEquals(sessionId, defaultRequestSessionId);
        executeJavaScriptApi(webContents, TERMINATE_SESSION_SCRIPT);
    }

    @Test
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    @Feature({"MediaRouter"})
    @LargeTest
    public void testSendAndOnMessage() {
        mActivityTestRule.loadUrl(mTestServer.getURL(TEST_PAGE));
        WebContents webContents = mActivityTestRule.getWebContents();
        executeJavaScriptApi(webContents, WAIT_DEVICE_SCRIPT);
        executeJavaScriptApi(webContents, START_SESSION_SCRIPT);
        View testRouteButton = RouterTestUtils.waitForRouteButton(
                mActivityTestRule.getActivity(), TEST_SINK_NAME, VIEW_TIMEOUT_MS, VIEW_RETRY_MS);
        ClickUtils.mouseSingleClickView(
                InstrumentationRegistry.getInstrumentation(), testRouteButton);
        executeJavaScriptApi(webContents, CHECK_SESSION_SCRIPT);
        String sessionId = getJavaScriptVariable(webContents, "startedConnection.id");
        Assert.assertFalse(sessionId.length() == 0);
        executeJavaScriptApi(
                webContents, String.format(SEND_MESSAGE_AND_EXPECT_RESPONSE_SCRIPT, "foo"));
    }

    @Test
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    @Feature({"MediaRouter"})
    @LargeTest
    @RetryOnFailure
    public void testOnClose() {
        MockMediaRouteProvider.Factory.sProvider.setCloseRouteWithErrorOnSend(true);
        mActivityTestRule.loadUrl(mTestServer.getURL(TEST_PAGE));
        WebContents webContents = mActivityTestRule.getWebContents();
        executeJavaScriptApi(webContents, WAIT_DEVICE_SCRIPT);
        executeJavaScriptApi(webContents, START_SESSION_SCRIPT);
        View testRouteButton = RouterTestUtils.waitForRouteButton(
                mActivityTestRule.getActivity(), TEST_SINK_NAME, VIEW_TIMEOUT_MS, VIEW_RETRY_MS);
        ClickUtils.mouseSingleClickView(
                InstrumentationRegistry.getInstrumentation(), testRouteButton);
        executeJavaScriptApi(webContents, CHECK_SESSION_SCRIPT);
        String sessionId = getJavaScriptVariable(webContents, "startedConnection.id");
        Assert.assertFalse(sessionId.length() == 0);
        executeJavaScriptApi(webContents, SEND_MESSAGE_AND_EXPECT_CONNECTION_CLOSE_ON_ERROR_SCRIPT);
    }

    @Test
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    @Feature({"MediaRouter"})
    @LargeTest
    @RetryOnFailure
    public void testFailNoProvider() {
        MockMediaRouteProvider.Factory.sProvider.setIsSupportsSource(false);
        mActivityTestRule.loadUrl(mTestServer.getURL(TEST_PAGE));
        WebContents webContents = mActivityTestRule.getWebContents();
        executeJavaScriptApi(webContents, WAIT_DEVICE_SCRIPT);
        executeJavaScriptApi(webContents, START_SESSION_SCRIPT);
        View testRouteButton = RouterTestUtils.waitForRouteButton(
                mActivityTestRule.getActivity(), TEST_SINK_NAME, VIEW_TIMEOUT_MS, VIEW_RETRY_MS);
        ClickUtils.mouseSingleClickView(
                InstrumentationRegistry.getInstrumentation(), testRouteButton);
        checkStartFailed(
                webContents, "UnknownError", "No provider supports createRoute with source");
    }

    @Test
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    @Feature({"MediaRouter"})
    @LargeTest
    @RetryOnFailure
    public void testFailCreateRoute() {
        MockMediaRouteProvider.Factory.sProvider.setCreateRouteErrorMessage("Unknown sink");
        mActivityTestRule.loadUrl(mTestServer.getURL(TEST_PAGE));
        WebContents webContents = mActivityTestRule.getWebContents();
        executeJavaScriptApi(webContents, WAIT_DEVICE_SCRIPT);
        executeJavaScriptApi(webContents, START_SESSION_SCRIPT);
        View testRouteButton = RouterTestUtils.waitForRouteButton(
                mActivityTestRule.getActivity(), TEST_SINK_NAME, VIEW_TIMEOUT_MS, VIEW_RETRY_MS);
        ClickUtils.mouseSingleClickView(
                InstrumentationRegistry.getInstrumentation(), testRouteButton);
        checkStartFailed(webContents, "UnknownError", "Unknown sink");
    }

    @Test
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    @Feature({"MediaRouter"})
    @LargeTest
    @RetryOnFailure
    public void testReconnectSession() {
        mActivityTestRule.loadUrl(mTestServer.getURL(TEST_PAGE));
        WebContents webContents = mActivityTestRule.getWebContents();
        executeJavaScriptApi(webContents, WAIT_DEVICE_SCRIPT);
        executeJavaScriptApi(webContents, START_SESSION_SCRIPT);
        View testRouteButton = RouterTestUtils.waitForRouteButton(
                mActivityTestRule.getActivity(), TEST_SINK_NAME, VIEW_TIMEOUT_MS, VIEW_RETRY_MS);
        ClickUtils.mouseSingleClickView(
                InstrumentationRegistry.getInstrumentation(), testRouteButton);
        executeJavaScriptApi(webContents, CHECK_SESSION_SCRIPT);
        String sessionId = getJavaScriptVariable(webContents, "startedConnection.id");

        mActivityTestRule.loadUrlInNewTab(mTestServer.getURL(TEST_PAGE));
        WebContents newWebContents = mActivityTestRule.getWebContents();
        Assert.assertTrue(webContents != newWebContents);
        executeJavaScriptApi(newWebContents, String.format("reconnectSession(\'%s\');", sessionId));
        String reconnectedSessionId =
                getJavaScriptVariable(newWebContents, "reconnectedSession.id");
        Assert.assertEquals(sessionId, reconnectedSessionId);
        executeJavaScriptApi(webContents, TERMINATE_SESSION_SCRIPT);
    }

    @Test
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    @Feature({"MediaRouter"})
    @LargeTest
    @RetryOnFailure
    public void testFailReconnectSession() {
        mActivityTestRule.loadUrl(mTestServer.getURL(TEST_PAGE));
        WebContents webContents = mActivityTestRule.getWebContents();
        executeJavaScriptApi(webContents, WAIT_DEVICE_SCRIPT);
        executeJavaScriptApi(webContents, START_SESSION_SCRIPT);
        View testRouteButton = RouterTestUtils.waitForRouteButton(
                mActivityTestRule.getActivity(), TEST_SINK_NAME, VIEW_TIMEOUT_MS, VIEW_RETRY_MS);
        ClickUtils.mouseSingleClickView(
                InstrumentationRegistry.getInstrumentation(), testRouteButton);
        executeJavaScriptApi(webContents, CHECK_SESSION_SCRIPT);
        String sessionId = getJavaScriptVariable(webContents, "startedConnection.id");

        MockMediaRouteProvider.Factory.sProvider.setJoinRouteErrorMessage("Unknown route");
        mActivityTestRule.loadUrlInNewTab(mTestServer.getURL(TEST_PAGE_RECONNECT_FAIL));
        WebContents newWebContents = mActivityTestRule.getWebContents();
        Assert.assertTrue(webContents != newWebContents);
        executeJavaScriptApi(
                newWebContents, String.format("checkReconnectSessionFails('%s');", sessionId));
    }

    @Test
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    @Feature({"MediaRouter"})
    @LargeTest
    @RetryOnFailure
    public void testFailStartCancelled() {
        mActivityTestRule.loadUrl(mTestServer.getURL(TEST_PAGE));
        WebContents webContents = mActivityTestRule.getWebContents();
        executeJavaScriptApi(webContents, WAIT_DEVICE_SCRIPT);
        executeJavaScriptApi(webContents, START_SESSION_SCRIPT);
        final Dialog routeSelectionDialog = RouterTestUtils.waitForDialog(
                mActivityTestRule.getActivity(), VIEW_TIMEOUT_MS, VIEW_RETRY_MS);
        Assert.assertNotNull(routeSelectionDialog);
        TestThreadUtils.runOnUiThreadBlocking(() -> { routeSelectionDialog.cancel(); });
        checkStartFailed(webContents, "NotAllowedError", "Dialog closed.");
    }
}
