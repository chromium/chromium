// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwWebContentsDelegate;
import org.chromium.base.test.util.Feature;

/** Tests for the ContentViewClient.addMessageToConsole() method. */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
public class ClientAddMessageToConsoleTest extends AwParameterizedTest {
    @Rule public AwActivityTestRule mActivityTestRule;

    // Line number at which the console message is logged in the page returned by the
    // getLogMessageJavaScriptData method.
    private static final int LOG_MESSAGE_JAVASCRIPT_DATA_LINE_NUMBER = 4;

    private static final String TEST_MESSAGE_ONE = "Test message one.";
    private static final String TEST_MESSAGE_TWO = "The second test message.";

    private TestAwContentsClient mContentsClient;
    private AwContents mAwContents;

    public ClientAddMessageToConsoleTest(AwSettingsMutation param) {
        this.mActivityTestRule = new AwActivityTestRule(param.getMutation());
    }

    @Before
    public void setUp() {
        mContentsClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = testContainerView.getAwContents();

        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(() -> mAwContents.getSettings().setJavaScriptEnabled(true));
    }

    private static String getLogMessageJavaScriptData(
            String consoleLogMethod, String message, boolean quoteMessage) {
        // The %0A sequence is an encoded newline and is needed to test the source line number.
        String logMessage = message;
        if (quoteMessage) logMessage = "'" + logMessage + "'";
        return "<html>%0A"
                + "<body>%0A"
                + "  <script>%0A"
                + "  console."
                + consoleLogMethod
                + "("
                + logMessage
                + ");%0A"
                + "  </script>%0A"
                + "  <div>%0A"
                + "Logging the message ["
                + message
                + "] using console."
                + consoleLogMethod
                + " method. "
                + "  </div>%0A"
                + "</body>%0A"
                + "</html>";
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testAddMessageToConsoleCalledWithCorrectLevel() throws Throwable {
        TestAwContentsClient.AddMessageToConsoleHelper addMessageToConsoleHelper =
                mContentsClient.getAddMessageToConsoleHelper();

        int callCount = addMessageToConsoleHelper.getCallCount();
        mActivityTestRule.loadDataSync(
                mAwContents,
                mContentsClient.getOnPageFinishedHelper(),
                getLogMessageJavaScriptData("error", "msg", true),
                "text/html",
                false);
        addMessageToConsoleHelper.waitForCallback(callCount);
        Assert.assertEquals(
                AwWebContentsDelegate.LOG_LEVEL_ERROR, addMessageToConsoleHelper.getLevel());

        callCount = addMessageToConsoleHelper.getCallCount();
        mActivityTestRule.loadDataSync(
                mAwContents,
                mContentsClient.getOnPageFinishedHelper(),
                getLogMessageJavaScriptData("warn", "msg", true),
                "text/html",
                false);
        addMessageToConsoleHelper.waitForCallback(callCount);
        Assert.assertEquals(
                AwWebContentsDelegate.LOG_LEVEL_WARNING, addMessageToConsoleHelper.getLevel());

        callCount = addMessageToConsoleHelper.getCallCount();
        mActivityTestRule.loadDataSync(
                mAwContents,
                mContentsClient.getOnPageFinishedHelper(),
                getLogMessageJavaScriptData("log", "msg", true),
                "text/html",
                false);
        addMessageToConsoleHelper.waitForCallback(callCount);
        Assert.assertEquals(
                AwWebContentsDelegate.LOG_LEVEL_LOG, addMessageToConsoleHelper.getLevel());

        // Can't test LOG_LEVEL_TIP as there's no way to generate a message at that log level
        // directly using JavaScript.
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testAddMessageToConsoleCalledWithCorrectMessage() throws Throwable {
        TestAwContentsClient.AddMessageToConsoleHelper addMessageToConsoleHelper =
                mContentsClient.getAddMessageToConsoleHelper();

        int callCount = addMessageToConsoleHelper.getCallCount();
        mActivityTestRule.loadDataSync(
                mAwContents,
                mContentsClient.getOnPageFinishedHelper(),
                getLogMessageJavaScriptData("log", TEST_MESSAGE_ONE, true),
                "text/html",
                false);
        addMessageToConsoleHelper.waitForCallback(callCount);
        Assert.assertEquals(TEST_MESSAGE_ONE, addMessageToConsoleHelper.getMessage());

        callCount = addMessageToConsoleHelper.getCallCount();
        mActivityTestRule.loadDataSync(
                mAwContents,
                mContentsClient.getOnPageFinishedHelper(),
                getLogMessageJavaScriptData("log", TEST_MESSAGE_TWO, true),
                "text/html",
                false);
        addMessageToConsoleHelper.waitForCallback(callCount);
        Assert.assertEquals(TEST_MESSAGE_TWO, addMessageToConsoleHelper.getMessage());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testAddMessageToConsoleCalledWithCorrectLineAndSource() throws Throwable {
        TestAwContentsClient.AddMessageToConsoleHelper addMessageToConsoleHelper =
                mContentsClient.getAddMessageToConsoleHelper();

        int callCount = addMessageToConsoleHelper.getCallCount();
        String data = getLogMessageJavaScriptData("log", TEST_MESSAGE_ONE, true);
        mActivityTestRule.loadDataSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), data, "text/html", false);
        addMessageToConsoleHelper.waitForCallback(callCount);
        Assert.assertTrue(
                "Url ["
                        + addMessageToConsoleHelper.getSourceId()
                        + "] expected to end with ["
                        + data
                        + "].",
                addMessageToConsoleHelper.getSourceId().endsWith(data));
        Assert.assertEquals(
                LOG_MESSAGE_JAVASCRIPT_DATA_LINE_NUMBER, addMessageToConsoleHelper.getLineNumber());
    }
}
