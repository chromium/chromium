// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.chromium.base.test.util.ScalableTimeout.scaleTimeout;

import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;
import android.webkit.JavascriptInterface;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.test.util.CommonResources;
import org.chromium.base.ThreadUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.content_public.browser.MessagePayload;
import org.chromium.content_public.browser.MessagePort;
import org.chromium.content_public.browser.test.util.TestCallbackHelperContainer.OnPageFinishedHelper;
import org.chromium.net.test.util.TestWebServer;

import java.io.UnsupportedEncodingException;
import java.util.Random;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicReference;

/** The tests for content postMessage API. */
@Batch(Batch.PER_CLASS)
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
public class PostMessageTest extends AwParameterizedTest {
    @Rule public AwActivityTestRule mActivityTestRule;

    private static final String SOURCE_ORIGIN = "";
    // Timeout to failure, in milliseconds
    private static final long TIMEOUT = scaleTimeout(5000);

    // Inject to the page to verify received messages.
    private static class MessageObject {
        private LinkedBlockingQueue<Data> mQueue = new LinkedBlockingQueue<>();

        public static class Data {
            public String mMessage;
            public String mOrigin;
            public int[] mPorts;

            public Data(String message, String origin, int[] ports) {
                mMessage = message;
                mOrigin = origin;
                mPorts = ports;
            }
        }

        @JavascriptInterface
        public void setMessageParams(String message, String origin, int[] ports) {
            mQueue.add(new Data(message, origin, ports));
        }

        public Data waitForMessage() throws Exception {
            return AwActivityTestRule.waitForNextQueueElement(mQueue);
        }
    }

    private static class ChannelContainer {
        private MessagePort[] mChannel;
        private LinkedBlockingQueue<Data> mQueue = new LinkedBlockingQueue<>();

        public static class Data {
            public MessagePayload mMessagePayload;
            public Looper mLastLooper;

            public Data(MessagePayload messagePayload, Looper looper) {
                mMessagePayload = messagePayload;
                mLastLooper = looper;
            }

            public String getStringValue() {
                return mMessagePayload.getAsString();
            }

            public byte[] getArrayBuffer() {
                return mMessagePayload.getAsArrayBuffer();
            }
        }

        public void set(MessagePort[] channel) {
            mChannel = channel;
        }

        public MessagePort[] get() {
            return mChannel;
        }

        public void notifyCalled(MessagePayload messagePayload) {
            try {
                mQueue.add(new Data(messagePayload, Looper.myLooper()));
            } catch (IllegalStateException e) {
                // We expect this add operation will always succeed since the default capacity of
                // the queue is Integer.MAX_VALUE.
            }
        }

        public Data waitForMessageCallback() throws Exception {
            return AwActivityTestRule.waitForNextQueueElement(mQueue);
        }

        public boolean isQueueEmpty() {
            return mQueue.isEmpty();
        }
    }

    private MessageObject mMessageObject;
    private TestAwContentsClient mContentsClient;
    private AwTestContainerView mTestContainerView;
    private AwContents mAwContents;
    private TestWebServer mWebServer;

    public PostMessageTest(AwSettingsMutation param) {
        this.mActivityTestRule = new AwActivityTestRule(param.getMutation());
    }

    @Before
    public void setUp() throws Exception {
        mMessageObject = new MessageObject();
        mContentsClient = new TestAwContentsClient();
        mTestContainerView = mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = mTestContainerView.getAwContents();
        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);

        try {
            AwActivityTestRule.addJavascriptInterfaceOnUiThread(
                    mAwContents, mMessageObject, "messageObject");
        } catch (Throwable t) {
            throw new RuntimeException(t);
        }
        mWebServer = TestWebServer.start();
    }

    @After
    public void tearDown() {
        mWebServer.shutdown();
    }

    private static final String WEBVIEW_MESSAGE = "from_webview";
    private static final String JS_MESSAGE = "from_js";

    private static final String TEST_PAGE =
            "<!DOCTYPE html><html><body>"
                    + "    <script>"
                    + "        onmessage = function (e) {"
                    + "            messageObject.setMessageParams(e.data, e.origin, e.ports);"
                    + "            if (e.ports != null && e.ports.length > 0) {"
                    + "               e.ports[0].postMessage(\""
                    + JS_MESSAGE
                    + "\");"
                    + "            }"
                    + "        }"
                    + "   </script>"
                    + "</body></html>";

    // Concats all the data fields of the received messages and makes it
    // available as page title.
    private static final String TITLE_FROM_POSTMESSAGE_TO_FRAME =
            "<!DOCTYPE html><html><body>"
                    + "    <script>"
                    + "        var received = '';"
                    + "        onmessage = function (e) {"
                    + "            received += e.data;"
                    + "            document.title = received;"
                    + "        }"
                    + "   </script>"
                    + "</body></html>";
    // Concats all the data fields of the received messages and makes it
    // available as page title.
    private static final String TITLE_FROM_POSTMESSAGE_TO_FRAME_ARRAYBUFFER =
            "<!DOCTYPE html><html><body>"
                    + "    <script>"
                    + "        var received = '';"
                    + "        onmessage = function (e) {"
                    + "            const view = new Int8Array(e.data);"
                    + "            received += String.fromCharCode.apply(null, view);"
                    + "            document.title = received;"
                    + "        }"
                    + "   </script>"
                    + "</body></html>";

    // Concats all the data fields of the received messages to the transferred channel
    // and makes it available as page title.
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
    // Concats all the data fields of the received messages to the transferred channel
    // and makes it available as page title.
    private static final String TITLE_FROM_POSTMESSAGE_TO_CHANNEL_ARRAYBUFFER =
            "<!DOCTYPE html><html><body>"
                    + "    <script>"
                    + "        onmessage = function (e) {"
                    + "            var myport = e.ports[0];"
                    + "            myport.onmessage = function (f) {"
                    + "                const view = new Int8Array(f.data);"
                    + "                document.title = String.fromCharCode.apply(null, view);"
                    + "            }"
                    + "        }"
                    + "   </script>"
                    + "</body></html>";

    // Call on non-UI thread.
    private void expectTitle(String title) {
        CriteriaHelper.pollUiThread(
                () -> Criteria.checkThat(mAwContents.getTitle(), Matchers.is(title)));
    }

    private void loadPage(String page) throws Throwable {
        final String url =
                mWebServer.setResponse(
                        "/test.html", page, CommonResources.getTextHtmlHeaders(true));
        OnPageFinishedHelper onPageFinishedHelper = mContentsClient.getOnPageFinishedHelper();
        int currentCallCount = onPageFinishedHelper.getCallCount();
        mActivityTestRule.loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), url);
        onPageFinishedHelper.waitForCallback(currentCallCount);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-PostMessage"})
    public void testPostMessageToMainFrame() throws Throwable {
        verifyPostMessageToMainFrame(mWebServer.getBaseUrl());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-PostMessage"})
    public void testPostMessageToMainFrameUsingWildcard() throws Throwable {
        verifyPostMessageToMainFrame("*");
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-PostMessage"})
    public void testPostMessageToMainFrameUsingEmptyStringAsWildcard() throws Throwable {
        verifyPostMessageToMainFrame("");
    }

    private void verifyPostMessageToMainFrame(final String targetOrigin) throws Throwable {
        loadPage(TEST_PAGE);
        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(
                        () ->
                                mAwContents.postMessageToMainFrame(
                                        new MessagePayload(WEBVIEW_MESSAGE), targetOrigin, null));
        MessageObject.Data data = mMessageObject.waitForMessage();
        Assert.assertEquals(WEBVIEW_MESSAGE, data.mMessage);
        Assert.assertEquals(SOURCE_ORIGIN, data.mOrigin);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-PostMessage"})
    public void testPostArrayBuffer() throws Throwable {
        loadPage(TITLE_FROM_POSTMESSAGE_TO_FRAME_ARRAYBUFFER);
        final String testString = "TestString";
        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(
                        () -> {
                            try {
                                mAwContents.postMessageToMainFrame(
                                        new MessagePayload(testString.getBytes("UTF-8")),
                                        mWebServer.getBaseUrl(),
                                        null);
                            } catch (UnsupportedEncodingException e) {
                                throw new RuntimeException(e);
                            }
                        });
        expectTitle(testString);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-PostMessage"})
    public void testPostArrayBufferOnMessagePort() throws Throwable {
        loadPage(TITLE_FROM_POSTMESSAGE_TO_CHANNEL_ARRAYBUFFER);
        final String testString = "TestString";
        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(
                        () -> {
                            MessagePort[] channel = mAwContents.createMessageChannel();
                            mAwContents.postMessageToMainFrame(
                                    new MessagePayload("1"),
                                    mWebServer.getBaseUrl(),
                                    new MessagePort[] {channel[1]});
                            try {
                                channel[0].postMessage(
                                        new MessagePayload(testString.getBytes("UTF-8")), null);
                            } catch (UnsupportedEncodingException e) {
                            }
                            channel[0].close();
                        });
        expectTitle(testString);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-PostMessage"})
    public void testTransferringSamePortTwiceViaPostMessageToMainFrameNotAllowed()
            throws Throwable {
        loadPage(TEST_PAGE);
        final CountDownLatch latch = new CountDownLatch(1);
        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(
                        () -> {
                            MessagePort[] channel = mAwContents.createMessageChannel();
                            mAwContents.postMessageToMainFrame(
                                    new MessagePayload("1"),
                                    mWebServer.getBaseUrl(),
                                    new MessagePort[] {channel[1]});
                            // Retransfer the port. This should fail with an exception.
                            try {
                                mAwContents.postMessageToMainFrame(
                                        new MessagePayload("2"),
                                        mWebServer.getBaseUrl(),
                                        new MessagePort[] {channel[1]});
                            } catch (IllegalStateException ex) {
                                latch.countDown();
                                return;
                            }
                            Assert.fail();
                        });
        boolean ignore = latch.await(TIMEOUT, TimeUnit.MILLISECONDS);
    }

    // There are two cases that put a port in a started state.
    // 1. posting a message
    // 2. setting an event handler.
    // A started port cannot return to "non-started" state. The four tests below verifies
    // these conditions for both conditions, using message ports and message channels.
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-PostMessage"})
    public void testStartedPortCannotBeTransferredUsingPostMessageToMainFrame1() throws Throwable {
        loadPage(TEST_PAGE);
        final CountDownLatch latch = new CountDownLatch(1);
        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(
                        () -> {
                            MessagePort[] channel = mAwContents.createMessageChannel();
                            channel[1].postMessage(new MessagePayload("1"), null);
                            try {
                                mAwContents.postMessageToMainFrame(
                                        new MessagePayload("2"),
                                        mWebServer.getBaseUrl(),
                                        new MessagePort[] {channel[1]});
                            } catch (IllegalStateException ex) {
                                latch.countDown();
                                return;
                            }
                            Assert.fail();
                        });
        boolean ignore = latch.await(TIMEOUT, TimeUnit.MILLISECONDS);
    }

    // see documentation in testStartedPortCannotBeTransferredUsingPostMessageToMainFrame1
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-PostMessage"})
    public void testStartedPortCannotBeTransferredUsingPostMessageToMainFrame2() throws Throwable {
        loadPage(TEST_PAGE);
        final CountDownLatch latch = new CountDownLatch(1);
        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(
                        () -> {
                            MessagePort[] channel = mAwContents.createMessageChannel();
                            // set a web event handler, this puts the port in a started state.
                            channel[1].setMessageCallback((message, sentPorts) -> {}, null);
                            try {
                                mAwContents.postMessageToMainFrame(
                                        new MessagePayload("2"),
                                        mWebServer.getBaseUrl(),
                                        new MessagePort[] {channel[1]});
                            } catch (IllegalStateException ex) {
                                latch.countDown();
                                return;
                            }
                            Assert.fail();
                        });
        boolean ignore = latch.await(TIMEOUT, TimeUnit.MILLISECONDS);
    }

    // see documentation in testStartedPortCannotBeTransferredUsingPostMessageToMainFrame1
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-PostMessage"})
    public void testStartedPortCannotBeTransferredUsingMessageChannel1() throws Throwable {
        loadPage(TEST_PAGE);
        final CountDownLatch latch = new CountDownLatch(1);
        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(
                        () -> {
                            MessagePort[] channel1 = mAwContents.createMessageChannel();
                            channel1[1].postMessage(new MessagePayload("1"), null);
                            MessagePort[] channel2 = mAwContents.createMessageChannel();
                            try {
                                channel2[0].postMessage(
                                        new MessagePayload("2"), new MessagePort[] {channel1[1]});
                            } catch (IllegalStateException ex) {
                                latch.countDown();
                                return;
                            }
                            Assert.fail();
                        });
        boolean ignore = latch.await(TIMEOUT, TimeUnit.MILLISECONDS);
    }

    // see documentation in testStartedPortCannotBeTransferredUsingPostMessageToMainFrame1
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-PostMessage"})
    public void testStartedPortCannotBeTransferredUsingMessageChannel2() throws Throwable {
        loadPage(TEST_PAGE);
        final CountDownLatch latch = new CountDownLatch(1);
        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(
                        () -> {
                            MessagePort[] channel1 = mAwContents.createMessageChannel();
                            // set a web event handler, this puts the port in a started state.
                            channel1[1].setMessageCallback((message, sentPorts) -> {}, null);
                            MessagePort[] channel2 = mAwContents.createMessageChannel();
                            try {
                                channel2[0].postMessage(
                                        new MessagePayload("1"), new MessagePort[] {channel1[1]});
                            } catch (IllegalStateException ex) {
                                latch.countDown();
                                return;
                            }
                            Assert.fail();
                        });
        boolean ignore = latch.await(TIMEOUT, TimeUnit.MILLISECONDS);
    }

    // channel[0] and channel[1] are entangled ports, establishing a channel. Verify
    // it is not allowed to transfer channel[0] on channel[0].postMessage.
    // TODO(sgurun) Note that the related case of posting channel[1] via
    // channel[0].postMessage does not throw a JS exception at present. We do not throw
    // an exception in this case either since the information of entangled port is not
    // available at the source port. We need a new mechanism to implement to prevent
    // this case.
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-PostMessage"})
    public void testTransferringSourcePortViaMessageChannelNotAllowed() throws Throwable {
        loadPage(TEST_PAGE);
        final CountDownLatch latch = new CountDownLatch(1);
        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(
                        () -> {
                            MessagePort[] channel = mAwContents.createMessageChannel();
                            try {
                                channel[0].postMessage(
                                        new MessagePayload("1"), new MessagePort[] {channel[0]});
                            } catch (IllegalStateException ex) {
                                latch.countDown();
                                return;
                            }
                            Assert.fail();
                        });
        boolean ignore = latch.await(TIMEOUT, TimeUnit.MILLISECONDS);
    }

    // Verify a closed port cannot be transferred to a frame.
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-PostMessage"})
    public void testSendClosedPortToFrameNotAllowed() throws Throwable {
        loadPage(TEST_PAGE);
        final CountDownLatch latch = new CountDownLatch(1);
        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(
                        () -> {
                            MessagePort[] channel = mAwContents.createMessageChannel();
                            channel[1].close();
                            try {
                                mAwContents.postMessageToMainFrame(
                                        new MessagePayload("1"),
                                        mWebServer.getBaseUrl(),
                                        new MessagePort[] {channel[1]});
                            } catch (IllegalStateException ex) {
                                latch.countDown();
                                return;
                            }
                            Assert.fail();
                        });
        boolean ignore = latch.await(TIMEOUT, TimeUnit.MILLISECONDS);
    }

    // Verify a closed port cannot be transferred to a port.
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-PostMessage"})
    public void testSendClosedPortToPortNotAllowed() throws Throwable {
        loadPage(TEST_PAGE);
        final CountDownLatch latch = new CountDownLatch(1);
        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(
                        () -> {
                            MessagePort[] channel1 = mAwContents.createMessageChannel();
                            MessagePort[] channel2 = mAwContents.createMessageChannel();
                            channel2[1].close();
                            try {
                                channel1[0].postMessage(
                                        new MessagePayload("1"), new MessagePort[] {channel2[1]});
                            } catch (IllegalStateException ex) {
                                latch.countDown();
                                return;
                            }
                            Assert.fail();
                        });
        boolean ignore = latch.await(TIMEOUT, TimeUnit.MILLISECONDS);
    }

    // Verify messages cannot be posted to closed ports.
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-PostMessage"})
    public void testPostMessageToClosedPortNotAllowed() throws Throwable {
        loadPage(TEST_PAGE);
        final CountDownLatch latch = new CountDownLatch(1);
        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(
                        () -> {
                            MessagePort[] channel = mAwContents.createMessageChannel();
                            channel[0].close();
                            try {
                                channel[0].postMessage(new MessagePayload("1"), null);
                            } catch (IllegalStateException ex) {
                                latch.countDown();
                                return;
                            }
                            Assert.fail();
                        });
        boolean ignore = latch.await(TIMEOUT, TimeUnit.MILLISECONDS);
    }

    // Verify messages posted before closing a port is received at the destination port.
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-PostMessage"})
    public void testMessagesPostedBeforeClosingPortAreTransferred() throws Throwable {
        loadPage(TITLE_FROM_POSTMESSAGE_TO_CHANNEL);
        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(
                        () -> {
                            MessagePort[] channel = mAwContents.createMessageChannel();
                            mAwContents.postMessageToMainFrame(
                                    new MessagePayload("1"),
                                    mWebServer.getBaseUrl(),
                                    new MessagePort[] {channel[1]});
                            channel[0].postMessage(new MessagePayload("2"), null);
                            channel[0].postMessage(new MessagePayload("3"), null);
                            channel[0].close();
                        });
        expectTitle("23");
    }

    // Verify a transferred port using postMessageToMainFrame cannot be closed.
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-PostMessage"})
    public void testClosingTransferredPortToFrameThrowsAnException() throws Throwable {
        loadPage(TEST_PAGE);
        final CountDownLatch latch = new CountDownLatch(1);
        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(
                        () -> {
                            MessagePort[] channel = mAwContents.createMessageChannel();
                            mAwContents.postMessageToMainFrame(
                                    new MessagePayload("1"),
                                    mWebServer.getBaseUrl(),
                                    new MessagePort[] {channel[1]});
                            try {
                                channel[1].close();
                            } catch (IllegalStateException ex) {
                                latch.countDown();
                                return;
                            }
                            Assert.fail();
                        });
        boolean ignore = latch.await(TIMEOUT, TimeUnit.MILLISECONDS);
    }

    // Verify a transferred port using postMessageToMainFrame cannot be closed.
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-PostMessage"})
    public void testClosingTransferredPortToChannelThrowsAnException() throws Throwable {
        loadPage(TEST_PAGE);
        final CountDownLatch latch = new CountDownLatch(1);
        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(
                        () -> {
                            MessagePort[] channel1 = mAwContents.createMessageChannel();
                            mAwContents.postMessageToMainFrame(
                                    new MessagePayload("1"),
                                    mWebServer.getBaseUrl(),
                                    new MessagePort[] {channel1[1]});
                            MessagePort[] channel2 = mAwContents.createMessageChannel();
                            channel1[0].postMessage(
                                    new MessagePayload("2"), new MessagePort[] {channel2[0]});
                            try {
                                channel2[0].close();
                            } catch (IllegalStateException ex) {
                                latch.countDown();
                                return;
                            }
                            Assert.fail();
                        });
        boolean ignore = latch.await(TIMEOUT, TimeUnit.MILLISECONDS);
    }

    // Create two message channels, and while they are in pending state, transfer the
    // second one in the first one.
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-PostMessage"})
    public void testPendingPortCanBeTransferredInPendingPort() throws Throwable {
        loadPage(TITLE_FROM_POSTMESSAGE_TO_CHANNEL);
        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(
                        () -> {
                            MessagePort[] channel1 = mAwContents.createMessageChannel();
                            mAwContents.postMessageToMainFrame(
                                    new MessagePayload("1"),
                                    mWebServer.getBaseUrl(),
                                    new MessagePort[] {channel1[1]});
                            MessagePort[] channel2 = mAwContents.createMessageChannel();
                            channel1[0].postMessage(
                                    new MessagePayload("2"), new MessagePort[] {channel2[0]});
                        });
        expectTitle("2");
    }

    private static final String ECHO_PAGE =
            "<!DOCTYPE html><html><body>"
                    + "    <script>"
                    + "        onmessage = function (e) {"
                    + "            var myPort = e.ports[0];"
                    + "            myPort.onmessage = function(e) {"
                    + "                myPort.postMessage(e.data + \""
                    + JS_MESSAGE
                    + "\"); }"
                    + "        }"
                    + "   </script>"
                    + "</body></html>";
    private static final String ECHO_ARRAY_BUFFER_PAGE =
            "<!DOCTYPE html><html><body>"
                    + "    <script>"
                    + "        onmessage = function (e) {"
                    + "            var myPort = e.ports[0];"
                    + "            myPort.onmessage = function(e) {"
                    + "                myPort.postMessage(e.data, [e.data]); }"
                    + "        }"
                    + "   </script>"
                    + "</body></html>";
    private static final String ECHO_NON_TRANFERABLE_ARRAY_BUFFER_PAGE =
            "<!DOCTYPE html><html><body>"
                    + "    <script>"
                    + "        onmessage = function (e) {"
                    + "            var myPort = e.ports[0];"
                    + "            myPort.onmessage = function(e) {"
                    + "                myPort.postMessage(e.data, [e.data]); }"
                    + "        }"
                    + "   </script>"
                    + "</body></html>";

    private static final String HELLO = "HELLO";

    // Message channels are created on UI thread. Verify that a message port
    // can be transferred to JS and full communication can happen on it. Do
    // this by sending a message to JS and letting it echo the message with
    // some text prepended to it.
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-PostMessage"})
    public void testMessageChannelUsingInitializedPort() throws Throwable {
        final ChannelContainer channelContainer = new ChannelContainer();
        loadPage(ECHO_PAGE);
        final MessagePort[] channel =
                ThreadUtils.runOnUiThreadBlocking(() -> mAwContents.createMessageChannel());

        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(
                        () -> {
                            channel[0].setMessageCallback(
                                    (message, sentPorts) -> channelContainer.notifyCalled(message),
                                    null);
                            mAwContents.postMessageToMainFrame(
                                    new MessagePayload(WEBVIEW_MESSAGE),
                                    mWebServer.getBaseUrl(),
                                    new MessagePort[] {channel[1]});
                            channel[0].postMessage(new MessagePayload(HELLO), null);
                        });
        // wait for the asynchronous response from JS
        ChannelContainer.Data data = channelContainer.waitForMessageCallback();
        Assert.assertEquals(HELLO + JS_MESSAGE, data.getStringValue());
    }

    // Verify that a message port can be used immediately (even if it is in
    // pending state) after creation. In particular make sure the message port can be
    // transferred to JS and full communication can happen on it.
    // Do this by sending a message to JS and let it echo'ing the message with
    // some text prepended to it.
    @SmallTest
    @Feature({"AndroidWebView", "Android-PostMessage"})
    @Test
    public void testMessageChannelUsingPendingPort() throws Throwable {
        final ChannelContainer channelContainer = new ChannelContainer();
        loadPage(ECHO_PAGE);
        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(
                        () -> {
                            MessagePort[] channel = mAwContents.createMessageChannel();
                            channel[0].setMessageCallback(
                                    (message, sentPorts) -> channelContainer.notifyCalled(message),
                                    null);
                            mAwContents.postMessageToMainFrame(
                                    new MessagePayload(WEBVIEW_MESSAGE),
                                    mWebServer.getBaseUrl(),
                                    new MessagePort[] {channel[1]});
                            channel[0].postMessage(new MessagePayload(HELLO), null);
                        });
        // Wait for the asynchronous response from JS.
        ChannelContainer.Data data = channelContainer.waitForMessageCallback();
        Assert.assertEquals(HELLO + JS_MESSAGE, data.getStringValue());
    }

    // Verify that a message port can be used for message transfer when both
    // ports are owned by same Webview.
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-PostMessage"})
    public void testMessageChannelCommunicationWithinWebView() throws Throwable {
        final ChannelContainer channelContainer = new ChannelContainer();
        loadPage(ECHO_PAGE);
        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(
                        () -> {
                            MessagePort[] channel = mAwContents.createMessageChannel();
                            channel[1].setMessageCallback(
                                    (message, sentPorts) -> channelContainer.notifyCalled(message),
                                    null);
                            channel[0].postMessage(new MessagePayload(HELLO), null);
                        });
        // Wait for the asynchronous response from JS.
        ChannelContainer.Data data = channelContainer.waitForMessageCallback();
        Assert.assertEquals(HELLO, data.getStringValue());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-PostMessage"})
    public void testMessageChannelSendAndReceiveArrayBuffer() throws Throwable {
        final byte[] bytes = HELLO.getBytes("UTF-8");
        verifyEchoArrayBuffer(ECHO_ARRAY_BUFFER_PAGE, bytes);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-PostMessage"})
    public void testMessageChannelSendAndReceiveLargeArrayBuffer() throws Throwable {
        final byte[] bytes = new byte[1000 * 1000]; // 1MB
        new Random(42).nextBytes(bytes);

        verifyEchoArrayBuffer(ECHO_ARRAY_BUFFER_PAGE, bytes);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-PostMessage"})
    public void testMessageChannelSendAndReceiveNonTransferableArrayBuffer() throws Throwable {
        final byte[] bytes = HELLO.getBytes("UTF-8");
        verifyEchoArrayBuffer(ECHO_NON_TRANFERABLE_ARRAY_BUFFER_PAGE, bytes);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-PostMessage"})
    public void testMessageChannelSendAndReceiveLargeNonTransferableArrayBuffer() throws Throwable {
        final byte[] bytes = new byte[1000 * 1000]; // 1MB
        new Random(42).nextBytes(bytes);

        verifyEchoArrayBuffer(ECHO_NON_TRANFERABLE_ARRAY_BUFFER_PAGE, bytes);
    }

    private void verifyEchoArrayBuffer(final String page, final byte[] bytes) throws Throwable {
        final ChannelContainer channelContainer = new ChannelContainer();
        loadPage(page);
        final MessagePort[] channel =
                ThreadUtils.runOnUiThreadBlocking(() -> mAwContents.createMessageChannel());

        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(
                        () -> {
                            channel[0].setMessageCallback(
                                    (message, sentPorts) -> channelContainer.notifyCalled(message),
                                    null);
                            mAwContents.postMessageToMainFrame(
                                    new MessagePayload(WEBVIEW_MESSAGE),
                                    mWebServer.getBaseUrl(),
                                    new MessagePort[] {channel[1]});
                            channel[0].postMessage(new MessagePayload(bytes), null);
                        });
        // wait for the asynchronous response from JS
        ChannelContainer.Data data = channelContainer.waitForMessageCallback();
        Assert.assertArrayEquals(bytes, data.getArrayBuffer());
    }

    // Post a message with a pending port to a frame and then post a bunch of messages
    // after that. Make sure that they are not ordered at the receiver side.
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-PostMessage"})
    public void testPostMessageToMainFrameNotReordersMessages() throws Throwable {
        loadPage(TITLE_FROM_POSTMESSAGE_TO_FRAME);
        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(
                        () -> {
                            MessagePort[] channel = mAwContents.createMessageChannel();
                            mAwContents.postMessageToMainFrame(
                                    new MessagePayload("1"),
                                    mWebServer.getBaseUrl(),
                                    new MessagePort[] {channel[1]});
                            mAwContents.postMessageToMainFrame(
                                    new MessagePayload("2"), mWebServer.getBaseUrl(), null);
                            mAwContents.postMessageToMainFrame(
                                    new MessagePayload("3"), mWebServer.getBaseUrl(), null);
                        });
        expectTitle("123");
    }

    // Generate an arraybuffer with a given size, and fill with ordered number, 0-255.
    // Then pass it back over MessagePort.
    private static final String GENERATE_ARRAY_BUFFER_FROM_JS_PAGE =
            "<!DOCTYPE html><html><body>"
                    + "    <script>"
                    + "        onmessage = function (e) {"
                    + "            var myPort = e.ports[0];"
                    + "            myPort.onmessage = function(e) {"
                    + "                let length = parseInt(e.data, 10);"
                    + "                var arrayBuffer = new ArrayBuffer(length);"
                    + "                const view = new Uint8Array(arrayBuffer);"
                    + "                for (var i = 0; i < length; ++i) {"
                    + "                    view[i] = i;"
                    + "                }"
                    + "                myPort.postMessage(arrayBuffer, [arrayBuffer]);"
                    + "            };"
                    + "        };"
                    + "    </script>";

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-PostMessage"})
    public void testReceiveArrayBufferFromJsOverMessagePort() throws Throwable {
        final int bufferLength = 5000;
        final byte[] expectedBytes = new byte[bufferLength];
        for (int i = 0; i < bufferLength; ++i) {
            // Cast to byte implicitly % 256.
            expectedBytes[i] = (byte) i;
        }

        final ChannelContainer channelContainer = new ChannelContainer();
        loadPage(GENERATE_ARRAY_BUFFER_FROM_JS_PAGE);
        final MessagePort[] channel =
                ThreadUtils.runOnUiThreadBlocking(() -> mAwContents.createMessageChannel());

        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(
                        () -> {
                            channel[0].setMessageCallback(
                                    (message, sentPorts) -> channelContainer.notifyCalled(message),
                                    null);
                            mAwContents.postMessageToMainFrame(
                                    new MessagePayload(WEBVIEW_MESSAGE),
                                    mWebServer.getBaseUrl(),
                                    new MessagePort[] {channel[1]});
                            channel[0].postMessage(
                                    new MessagePayload(String.valueOf(bufferLength)), null);
                        });
        // wait for the asynchronous response from JS
        ChannelContainer.Data data = channelContainer.waitForMessageCallback();
        final byte[] bytes = data.getArrayBuffer();
        Assert.assertEquals(bufferLength, bytes.length);
        Assert.assertArrayEquals(expectedBytes, bytes);
    }

    private static final String RECEIVE_JS_MESSAGE_CHANNEL_PAGE =
            "<!DOCTYPE html><html><body>"
                    + "    <script>"
                    + "        var received ='';"
                    + "        var mc = new MessageChannel();"
                    + "        mc.port1.onmessage = function (e) {"
                    + "            received += e.data;"
                    + "            document.title = received;"
                    + "            if (e.data == '2') { mc.port1.postMessage('3'); }"
                    + "        };"
                    + "        onmessage = function (e) {"
                    + "            var myPort = e.ports[0];"
                    + "            myPort.postMessage('from window', [mc.port2]);"
                    + "        }"
                    + "   </script>"
                    + "</body></html>";

    // Test webview can use a message port received from JS for full duplex communication.
    // Test steps:
    // 1. Java creates a message channel, and send one port to JS
    // 2. JS creates a new message channel and sends one port to Java using the channel in 1
    // 3. Java sends a message using the new channel in 2.
    // 4. Js responds to this message using the channel in 2.
    // 5. Java responds to message in 4 using the channel in 2.
    @SmallTest
    @Feature({"AndroidWebView", "Android-PostMessage"})
    @Test
    public void testCanUseReceivedAwMessagePortFromJS() throws Throwable {
        loadPage(RECEIVE_JS_MESSAGE_CHANNEL_PAGE);
        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(
                        () -> {
                            MessagePort[] channel = mAwContents.createMessageChannel();
                            mAwContents.postMessageToMainFrame(
                                    new MessagePayload("1"),
                                    mWebServer.getBaseUrl(),
                                    new MessagePort[] {channel[1]});
                            channel[0].setMessageCallback(
                                    (message, p) -> {
                                        p[0].setMessageCallback(
                                                (message1, q) -> {
                                                    Assert.assertEquals(
                                                            "3", message1.getAsString());
                                                    p[0].postMessage(new MessagePayload("4"), null);
                                                },
                                                null);
                                        p[0].postMessage(new MessagePayload("2"), null);
                                    },
                                    null);
                        });
        expectTitle("24");
    }

    private static final String WORKER_MESSAGE = "from_worker";

    // Listen for messages. Pass port 1 to worker and use port 2 to receive messages from
    // from worker.
    private static final String TEST_PAGE_FOR_PORT_TRANSFER =
            "<!DOCTYPE html><html><body>"
                    + "    <script>"
                    + "        var worker = new Worker(\"worker.js\");"
                    + "        onmessage = function (e) {"
                    + "            if (e.data == \""
                    + WEBVIEW_MESSAGE
                    + "\") {"
                    + "                worker.postMessage(\"worker_port\", [e.ports[0]]);"
                    + "                var messageChannelPort = e.ports[1];"
                    + "                messageChannelPort.onmessage = receiveWorkerMessage;"
                    + "            }"
                    + "        };"
                    + "        function receiveWorkerMessage(e) {"
                    + "            if (e.data == \""
                    + WORKER_MESSAGE
                    + "\") {"
                    + "                messageObject.setMessageParams(e.data, e.origin, e.ports);"
                    + "            }"
                    + "        };"
                    + "   </script>"
                    + "</body></html>";

    private static final String WORKER_SCRIPT =
            "onmessage = function(e) {"
                    + "    if (e.data == \"worker_port\") {"
                    + "        var toWindow = e.ports[0];"
                    + "        toWindow.postMessage(\""
                    + WORKER_MESSAGE
                    + "\");"
                    + "        toWindow.start();"
                    + "    }"
                    + "}";

    // Test if message ports created at the native side can be transferred
    // to JS side, to establish a communication channel between a worker and a frame.
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-PostMessage"})
    public void testTransferPortsToWorker() throws Throwable {
        mWebServer.setResponse(
                "/worker.js", WORKER_SCRIPT, CommonResources.getTextJavascriptHeaders(true));
        loadPage(TEST_PAGE_FOR_PORT_TRANSFER);
        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(
                        () -> {
                            MessagePort[] channel = mAwContents.createMessageChannel();
                            mAwContents.postMessageToMainFrame(
                                    new MessagePayload(WEBVIEW_MESSAGE),
                                    mWebServer.getBaseUrl(),
                                    new MessagePort[] {channel[0], channel[1]});
                        });
        MessageObject.Data data = mMessageObject.waitForMessage();
        Assert.assertEquals(WORKER_MESSAGE, data.mMessage);
    }

    private static final String POPUP_MESSAGE = "from_popup";
    private static final String POPUP_URL = "/popup.html";
    private static final String IFRAME_URL = "/iframe.html";
    private static final String MAIN_PAGE_FOR_POPUP_TEST =
            "<!DOCTYPE html><html>"
                    + "<head>"
                    + "    <script>"
                    + "        function createPopup() {"
                    + "            var popupWindow = window.open('"
                    + POPUP_URL
                    + "');"
                    + "            onmessage = function(e) {"
                    + "                popupWindow.postMessage(e.data, '*', e.ports);"
                    + "            };"
                    + "        }"
                    + "    </script>"
                    + "</head>"
                    + "</html>";

    // Sends message and ports to the iframe.
    private static final String POPUP_PAGE_WITH_IFRAME =
            "<!DOCTYPE html><html>"
                    + "<script>"
                    + "    onmessage = function(e) {"
                    + "        var iframe = document.getElementsByTagName('iframe')[0];"
                    + "        iframe.contentWindow.postMessage('"
                    + POPUP_MESSAGE
                    + "', '*', e.ports);"
                    + "    };"
                    + "</script>"
                    + "<body><iframe src='"
                    + IFRAME_URL
                    + "'></iframe></body>"
                    + "</html>";

    // Test if WebView can post a message from/to a popup window owning a message port.
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-PostMessage"})
    public void testPostMessageToPopup() throws Throwable {
        mActivityTestRule.triggerPopup(
                mAwContents,
                mContentsClient,
                mWebServer,
                MAIN_PAGE_FOR_POPUP_TEST,
                ECHO_PAGE,
                POPUP_URL,
                "createPopup()");
        mActivityTestRule.connectPendingPopup(mAwContents);
        final ChannelContainer channelContainer = new ChannelContainer();

        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(
                        () -> {
                            MessagePort[] channel = mAwContents.createMessageChannel();
                            channel[0].setMessageCallback(
                                    (message, sentPorts) -> channelContainer.notifyCalled(message),
                                    null);
                            mAwContents.postMessageToMainFrame(
                                    new MessagePayload(WEBVIEW_MESSAGE),
                                    mWebServer.getBaseUrl(),
                                    new MessagePort[] {channel[1]});
                            channel[0].postMessage(new MessagePayload(HELLO), null);
                        });
        ChannelContainer.Data data = channelContainer.waitForMessageCallback();
        Assert.assertEquals(HELLO + JS_MESSAGE, data.getStringValue());
    }

    // Test if WebView can post a message from/to an iframe in a popup window.
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-PostMessage"})
    public void testPostMessageToIframeInsidePopup() throws Throwable {
        mWebServer.setResponse(IFRAME_URL, ECHO_PAGE, null);
        mActivityTestRule.triggerPopup(
                mAwContents,
                mContentsClient,
                mWebServer,
                MAIN_PAGE_FOR_POPUP_TEST,
                POPUP_PAGE_WITH_IFRAME,
                POPUP_URL,
                "createPopup()");
        mActivityTestRule.connectPendingPopup(mAwContents);
        final ChannelContainer channelContainer = new ChannelContainer();

        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(
                        () -> {
                            MessagePort[] channel = mAwContents.createMessageChannel();
                            channel[0].setMessageCallback(
                                    (message, sentPorts) -> channelContainer.notifyCalled(message),
                                    null);
                            mAwContents.postMessageToMainFrame(
                                    new MessagePayload(WEBVIEW_MESSAGE),
                                    mWebServer.getBaseUrl(),
                                    new MessagePort[] {channel[1]});
                            channel[0].postMessage(new MessagePayload(HELLO), null);
                        });
        ChannelContainer.Data data = channelContainer.waitForMessageCallback();
        Assert.assertEquals(HELLO + JS_MESSAGE, data.getStringValue());
    }

    private static final String TEST_PAGE_FOR_UNSUPPORTED_MESSAGES =
            "<!DOCTYPE html><html><body>"
                    + "    <script>"
                    + "        onmessage = function (e) {"
                    + "            e.ports[0].postMessage(null);"
                    + "            e.ports[0].postMessage(undefined);"
                    + "            e.ports[0].postMessage(NaN);"
                    + "            e.ports[0].postMessage(0);"
                    + "            e.ports[0].postMessage(new Set());"
                    + "            e.ports[0].postMessage({});"
                    + "            e.ports[0].postMessage(['1','2','3']);"
                    + "            e.ports[0].postMessage('"
                    + JS_MESSAGE
                    + "');"
                    + "        }"
                    + "   </script>"
                    + "</body></html>";

    // Make sure that postmessage can handle unsupported messages gracefully.
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-PostMessage"})
    public void testPostUnsupportedWebMessageToApp() throws Throwable {
        loadPage(TEST_PAGE_FOR_UNSUPPORTED_MESSAGES);
        final ChannelContainer channelContainer = new ChannelContainer();
        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(
                        () -> {
                            MessagePort[] channel = mAwContents.createMessageChannel();
                            channel[0].setMessageCallback(
                                    (message, sentPorts) -> channelContainer.notifyCalled(message),
                                    null);
                            mAwContents.postMessageToMainFrame(
                                    new MessagePayload(WEBVIEW_MESSAGE),
                                    mWebServer.getBaseUrl(),
                                    new MessagePort[] {channel[1]});
                        });
        ChannelContainer.Data data = channelContainer.waitForMessageCallback();
        Assert.assertEquals(JS_MESSAGE, data.getStringValue());
        // Assert that onMessage is called only once.
        Assert.assertTrue(channelContainer.isQueueEmpty());
    }

    private static final String TEST_TRANSFER_EMPTY_PORTS =
            "<!DOCTYPE html><html><body>"
                    + "    <script>"
                    + "        onmessage = function (e) {"
                    + "            e.ports[0].postMessage('1', undefined);"
                    + "            e.ports[0].postMessage('2', []);"
                    + "        }"
                    + "   </script>"
                    + "</body></html>";

    // Make sure that postmessage can handle unsupported messages gracefully.
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-PostMessage"})
    public void testTransferEmptyPortsArray() throws Throwable {
        loadPage(TEST_TRANSFER_EMPTY_PORTS);
        final ChannelContainer channelContainer = new ChannelContainer();
        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(
                        () -> {
                            MessagePort[] channel = mAwContents.createMessageChannel();
                            channel[0].setMessageCallback(
                                    (message, sentPorts) -> channelContainer.notifyCalled(message),
                                    null);
                            mAwContents.postMessageToMainFrame(
                                    new MessagePayload(WEBVIEW_MESSAGE),
                                    mWebServer.getBaseUrl(),
                                    new MessagePort[] {channel[1]});
                        });
        ChannelContainer.Data data1 = channelContainer.waitForMessageCallback();
        Assert.assertEquals("1", data1.getStringValue());
        ChannelContainer.Data data2 = channelContainer.waitForMessageCallback();
        Assert.assertEquals("2", data2.getStringValue());
    }

    // Make sure very large messages can be sent and received.
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-PostMessage"})
    public void testVeryLargeMessage() throws Throwable {
        mWebServer.setResponse(IFRAME_URL, ECHO_PAGE, null);
        mActivityTestRule.triggerPopup(
                mAwContents,
                mContentsClient,
                mWebServer,
                MAIN_PAGE_FOR_POPUP_TEST,
                POPUP_PAGE_WITH_IFRAME,
                POPUP_URL,
                "createPopup()");
        mActivityTestRule.connectPendingPopup(mAwContents);
        final ChannelContainer channelContainer = new ChannelContainer();

        final StringBuilder longMessageBuilder = new StringBuilder();
        for (int i = 0; i < 100000; ++i) longMessageBuilder.append(HELLO);
        final String longMessage = longMessageBuilder.toString();

        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(
                        () -> {
                            MessagePort[] channel = mAwContents.createMessageChannel();
                            channel[0].setMessageCallback(
                                    (message, sentPorts) -> channelContainer.notifyCalled(message),
                                    null);
                            mAwContents.postMessageToMainFrame(
                                    new MessagePayload(WEBVIEW_MESSAGE),
                                    mWebServer.getBaseUrl(),
                                    new MessagePort[] {channel[1]});
                            channel[0].postMessage(new MessagePayload(longMessage), null);
                        });
        ChannelContainer.Data data = channelContainer.waitForMessageCallback();
        Assert.assertEquals(longMessage + JS_MESSAGE, data.getStringValue());
    }

    // Make sure messages are dispatched on the correct looper.
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-PostMessage"})
    public void testMessageOnCorrectLooper() throws Throwable {
        final ChannelContainer channelContainer1 = new ChannelContainer();
        final ChannelContainer channelContainer2 = new ChannelContainer();
        final HandlerThread thread = new HandlerThread("test-thread");
        thread.start();
        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(
                        () -> {
                            MessagePort[] channel = mAwContents.createMessageChannel();
                            channel[0].setMessageCallback(
                                    (message, sentPorts) -> channelContainer1.notifyCalled(message),
                                    null);
                            channel[1].setMessageCallback(
                                    (message, sentPorts) -> channelContainer2.notifyCalled(message),
                                    new Handler(thread.getLooper()));
                            channel[0].postMessage(new MessagePayload("foo"), null);
                            channel[1].postMessage(new MessagePayload("bar"), null);
                        });
        ChannelContainer.Data data1 = channelContainer1.waitForMessageCallback();
        ChannelContainer.Data data2 = channelContainer2.waitForMessageCallback();
        Assert.assertEquals("bar", data1.getStringValue());
        Assert.assertEquals(Looper.getMainLooper(), data1.mLastLooper);
        Assert.assertEquals("foo", data2.getStringValue());
        Assert.assertEquals(thread.getLooper(), data2.mLastLooper);
    }

    // Make sure it is possible to change the message handler.
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-PostMessage"})
    public void testChangeMessageHandler() throws Throwable {
        final ChannelContainer channelContainer = new ChannelContainer();
        final HandlerThread thread = new HandlerThread("test-thread");
        thread.start();
        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(
                        () -> {
                            MessagePort[] channel = mAwContents.createMessageChannel();
                            channelContainer.set(channel);
                            channel[0].setMessageCallback(
                                    (message, sentPorts) -> channelContainer.notifyCalled(message),
                                    new Handler(thread.getLooper()));
                            channel[1].postMessage(new MessagePayload("foo"), null);
                        });
        ChannelContainer.Data data = channelContainer.waitForMessageCallback();
        Assert.assertEquals("foo", data.getStringValue());
        Assert.assertEquals(thread.getLooper(), data.mLastLooper);
        final ChannelContainer channelContainer2 = new ChannelContainer();
        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(
                        () -> {
                            MessagePort[] channel = channelContainer.get();
                            channel[0].setMessageCallback(
                                    (message, sentPorts) -> channelContainer2.notifyCalled(message),
                                    null);
                            channel[1].postMessage(new MessagePayload("bar"), null);
                        });
        ChannelContainer.Data data2 = channelContainer2.waitForMessageCallback();
        Assert.assertEquals("bar", data2.getStringValue());
        Assert.assertEquals(Looper.getMainLooper(), data2.mLastLooper);
    }

    // Regression test for crbug.com/973901
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-PostMessage"})
    public void testPostMessageBeforePageLoadWontBlockNavigation() throws Throwable {
        final String baseUrl = mWebServer.getBaseUrl();

        // postMessage before page load.
        ThreadUtils.runOnUiThreadBlocking(
                () -> mAwContents.postMessageToMainFrame(new MessagePayload("1"), baseUrl, null));

        // loadPage shouldn't timeout.
        loadPage(TEST_PAGE);

        // Verify that after the page gets load, postMessage still works.
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mAwContents.postMessageToMainFrame(
                                new MessagePayload(WEBVIEW_MESSAGE), baseUrl, null));

        MessageObject.Data data = mMessageObject.waitForMessage();
        Assert.assertEquals(WEBVIEW_MESSAGE, data.mMessage);
        Assert.assertEquals(SOURCE_ORIGIN, data.mOrigin);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-PostMessage"})
    public void testMessagePortLifecycle() throws Throwable {
        final String baseUrl = mWebServer.getBaseUrl();
        loadPage(TEST_PAGE);
        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(
                        () -> {
                            final MessagePort[] ports = mAwContents.createMessageChannel();
                            Assert.assertFalse(ports[0].isTransferred());
                            Assert.assertFalse(ports[0].isClosed());
                            Assert.assertFalse(ports[0].isStarted());
                            Assert.assertFalse(ports[1].isTransferred());
                            Assert.assertFalse(ports[1].isClosed());
                            Assert.assertFalse(ports[1].isStarted());

                            // Post port1 to main frame.
                            mAwContents.postMessageToMainFrame(
                                    new MessagePayload("1"), baseUrl, new MessagePort[] {ports[1]});
                            Assert.assertTrue(ports[1].isTransferred());
                            Assert.assertFalse(ports[1].isClosed());
                            Assert.assertFalse(ports[1].isStarted());

                            // Close one port.
                            ports[0].close();
                            Assert.assertFalse(ports[0].isTransferred());
                            Assert.assertTrue(ports[0].isClosed());
                            Assert.assertFalse(ports[0].isStarted());
                        });
    }

    private static final String COUNT_PORT_FROM_MESSAGE =
            "<!DOCTYPE html><html><body>"
                    + "    <script>"
                    + "        var counter = 0;"
                    + "        var received = '';"
                    + "        onmessage = function (e) {"
                    + "            e.ports[0].onmessage = function(e) {"
                    + "                received += e.data;"
                    + "                counter += e.ports.length;"
                    + "                document.title = received + counter;"
                    + "                e.ports[0].postMessage(received + counter);"
                    + "            };"
                    + "        };"
                    + "   </script>"
                    + "</body></html>";

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-PostMessage"})
    // Previously postMessage can be called on any thread, but no tests or CTS tests checked.
    public void testTransferPortOnAnotherThread() throws Throwable {
        loadPage(COUNT_PORT_FROM_MESSAGE);
        final ChannelContainer container = new ChannelContainer();
        final MessagePort[] ports =
                ThreadUtils.runOnUiThreadBlocking(() -> mAwContents.createMessageChannel());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mAwContents.postMessageToMainFrame(
                            new MessagePayload(""), "*", new MessagePort[] {ports[1]});
                });
        final MessagePort[] ports2 =
                ThreadUtils.runOnUiThreadBlocking(() -> mAwContents.createMessageChannel());
        ports2[0].setMessageCallback(
                (messagePayload, sentPorts) -> {
                    ThreadUtils.checkUiThread();
                    container.notifyCalled(messagePayload);
                },
                null);
        ports[0].postMessage(new MessagePayload(HELLO), new MessagePort[] {ports2[1]});
        expectTitle(HELLO + "1");
        Assert.assertEquals(HELLO + "1", container.waitForMessageCallback().getStringValue());
        ports[0].close();
        ports2[0].close();
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-PostMessage"})
    public void testTransferPortImmediateAfterPostMessageOnAnotherThread() throws Throwable {
        loadPage(COUNT_PORT_FROM_MESSAGE);
        final MessagePort[] ports =
                ThreadUtils.runOnUiThreadBlocking(() -> mAwContents.createMessageChannel());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mAwContents.postMessageToMainFrame(
                            new MessagePayload(""), "*", new MessagePort[] {ports[1]});
                });
        final CallbackHelper callbackHelper = new CallbackHelper();
        final AtomicReference<IllegalStateException> exceptionRef = new AtomicReference<>();
        PostTask.postTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    try {
                        callbackHelper.waitForCallback(0);
                        mAwContents.postMessageToMainFrame(
                                new MessagePayload(HELLO), "*", new MessagePort[] {ports[0]});
                    } catch (TimeoutException ignored) {
                    } catch (IllegalStateException e) {
                        exceptionRef.set(e);
                        callbackHelper.notifyCalled();
                    }
                });
        ports[0].postMessage(new MessagePayload(HELLO), null);
        callbackHelper.notifyCalled();

        callbackHelper.waitForCallback(1);
        Assert.assertEquals("Port is already started", exceptionRef.get().getMessage());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-PostMessage"})
    public void testCloseMessagePortOnAnotherThread() throws Throwable {
        final MessagePort[] messagePorts = new MessagePort[1];
        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(
                        () -> {
                            final MessagePort[] ports = mAwContents.createMessageChannel();
                            messagePorts[0] = ports[0];
                            // Move message port into |receiving| state.
                            messagePorts[0].setMessageCallback(
                                    (messagePayload, sentPorts) -> {}, null);
                        });
        // Close message channel on another thread, simulate the case where the "finalize" is called
        // on finalizer thread.
        messagePorts[0].close();
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-PostMessage"})
    public void testTransferPortInAnotherThreadRaceCondition() throws Throwable {
        loadPage(COUNT_PORT_FROM_MESSAGE);
        final MessagePort[] ports =
                ThreadUtils.runOnUiThreadBlocking(() -> mAwContents.createMessageChannel());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mAwContents.postMessageToMainFrame(
                            new MessagePayload(""), "*", new MessagePort[] {ports[1]});
                });
        final MessagePort[] portsToTransfer =
                ThreadUtils.runOnUiThreadBlocking(() -> mAwContents.createMessageChannel());
        // Transfer the port in another thread.
        ports[0].postMessage(new MessagePayload("test"), new MessagePort[] {portsToTransfer[0]});
        // Check port2[0] is transferred right now.
        Assert.assertTrue(portsToTransfer[0].isTransferred());
        // Set callback on the just transferred port right now. It should fail.
        try {
            portsToTransfer[0].setMessageCallback((messagePayload, sentPorts) -> {}, null);
            Assert.fail("Port transferred, should not able to listen on");
        } catch (IllegalStateException e) {
            // Ignored.
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-PostMessage"})
    public void testSetReceiverAfterMessageReceived() throws Throwable {
        loadPage(COUNT_PORT_FROM_MESSAGE);
        final ChannelContainer container = new ChannelContainer();
        final HandlerThread thread = new HandlerThread("test-thread");
        thread.start();
        final Handler handler = new Handler(thread.getLooper());
        final MessagePort[] ports =
                ThreadUtils.runOnUiThreadBlocking(() -> mAwContents.createMessageChannel());

        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(
                        () -> {
                            // Post message before set callback
                            ports[0].postMessage(new MessagePayload("msg1"), null);
                        });
        ports[1].setMessageCallback(
                (messagePayload, sentPorts) -> {
                    container.notifyCalled(messagePayload);
                },
                handler);
        Assert.assertEquals("msg1", container.waitForMessageCallback().getStringValue());
    }

    private static final String COPY_PORT_MESSAGE_FROM_WINDOW =
            "<!DOCTYPE html><html><body>"
                    + "    <script>"
                    + "        var port = null;"
                    + "        onmessage = function (e) {"
                    + "            if (e.ports[0]) port = e.ports[0];"
                    + "            else port.postMessage(e.data);"
                    + "        };"
                    + "   </script>"
                    + "</body></html>";

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Android-PostMessage"})
    // Regression test of https://issuetracker.google.com/245837736
    public void testMessageListenerAvailableAfterPortGarbageCollected() throws Throwable {
        loadPage(COPY_PORT_MESSAGE_FROM_WINDOW);
        final ChannelContainer container = new ChannelContainer();
        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(
                        () -> {
                            MessagePort[] ports = mAwContents.createMessageChannel();
                            ports[0].setMessageCallback(
                                    (message, p) -> container.notifyCalled(message), null);
                            mAwContents.postMessageToMainFrame(
                                    new MessagePayload("*"), "*", new MessagePort[] {ports[1]});
                            ports = null;
                        });
        for (int i = 0; i < 100; ++i) {
            final String message = HELLO + i;
            Runtime.getRuntime().gc();
            InstrumentationRegistry.getInstrumentation()
                    .runOnMainSync(
                            () -> {
                                // Trigger GC to make ports[0] being garbage collected. Note that
                                // despite that what JavaDoc says about invoking "gc()", both
                                // Dalvik and ART actually run the collector.
                                mAwContents.postMessageToMainFrame(
                                        new MessagePayload(message), "*", null);
                            });
            Assert.assertEquals(message, container.waitForMessageCallback().getStringValue());
        }
    }
}
