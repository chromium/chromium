// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test.services;

import static org.chromium.android_webview.test.OnlyRunIn.ProcessMode.SINGLE_PROCESS;

import android.content.Context;

import androidx.javascriptengine.IsolateStartupParameters;
import androidx.javascriptengine.JavaScriptIsolate;
import androidx.javascriptengine.JavaScriptSandbox;
import androidx.javascriptengine.Message;
import androidx.javascriptengine.MessagePort;
import androidx.javascriptengine.MessagePortClient;
import androidx.javascriptengine.TerminationInfo;
import androidx.test.filters.LargeTest;

import com.google.common.util.concurrent.ListenableFuture;
import com.google.common.util.concurrent.MoreExecutors;

import org.junit.After;
import org.junit.Assert;
import org.junit.Assume;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.test.AwJUnit4ClassRunner;
import org.chromium.android_webview.test.OnlyRunIn;
import org.chromium.base.ContextUtils;
import org.chromium.base.test.util.Batch;

import java.util.Arrays;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.TimeUnit;

/**
 * Tests the behavior of MessagePorts with a focus on sandbox side details.
 *
 * <p>Most JavaScriptEngine tests are performed within AndroidX. These Chromium-side tests focus on
 * JavaScript-visible APIs.
 */
@RunWith(AwJUnit4ClassRunner.class)
@OnlyRunIn(SINGLE_PROCESS)
@Batch(Batch.PER_CLASS)
@LargeTest
public class JsSandboxMessagePortTest {
    private static final int TIMEOUT_SECONDS = 5;
    private static final String PORT_NAME = "TestPort";
    private static final int MAX_HEAP_SIZE = 64 * 1048576; // 64MiB
    private static final int MAX_EVALUATION_RETURN_SIZE = 1048576; // 1MiB
    private static final MessagePortClient EXPECT_NO_INCOMING_MESSAGES =
            message -> Assert.fail("Message received when none were expected.");

    private final Context mContext = ContextUtils.getApplicationContext();
    private JavaScriptSandbox mJsSandbox;
    private JavaScriptIsolate mJsIsolate;

    @Before
    public void setup() throws Throwable {
        Assume.assumeTrue(JavaScriptSandbox.isSupported());
        ListenableFuture<JavaScriptSandbox> jsSandboxFuture =
                JavaScriptSandbox.createConnectedInstanceForTestingAsync(mContext);
        mJsSandbox = jsSandboxFuture.get(TIMEOUT_SECONDS, TimeUnit.SECONDS);
        Assume.assumeTrue(
                mJsSandbox.isFeatureSupported(JavaScriptSandbox.JS_FEATURE_MESSAGE_PORTS));
        IsolateStartupParameters isolateStartupParameters = new IsolateStartupParameters();
        isolateStartupParameters.setMaxHeapSizeBytes(MAX_HEAP_SIZE);
        isolateStartupParameters.setMaxEvaluationReturnSizeBytes(MAX_EVALUATION_RETURN_SIZE);
        mJsIsolate = mJsSandbox.createIsolate(isolateStartupParameters);
    }

    @After
    public void teardown() {
        if (mJsIsolate != null) {
            mJsIsolate.close();
        }
        if (mJsSandbox != null) {
            mJsSandbox.close();
        }
    }

    @Test
    public void testPostMessageWithArgs_string_succeeds() throws Throwable {
        final BlockingQueue<Message> messageQueue = new LinkedBlockingQueue<>();
        MessagePort messagePort =
                mJsIsolate.provideMessagePort(
                        PORT_NAME, MoreExecutors.directExecutor(), messageQueue::add);
        String code =
                String.format(
                        """
                        (async () => {
                            const port = await android.getNamedPort('%s');
                            port.postMessage('string message');
                            return 'PASS';
                        })()
                        """,
                        PORT_NAME);
        ListenableFuture<String> result = mJsIsolate.evaluateJavaScriptAsync(code);
        Assert.assertEquals("PASS", result.get(TIMEOUT_SECONDS, TimeUnit.SECONDS));
        Assert.assertEquals(
                "string message", messageQueue.poll(TIMEOUT_SECONDS, TimeUnit.SECONDS).getString());
    }

    @Test
    public void testPostMessageWithArgs_arrayBuffer_succeeds() throws Throwable {
        final BlockingQueue<Message> messageQueue = new LinkedBlockingQueue<>();
        MessagePort messagePort =
                mJsIsolate.provideMessagePort(
                        PORT_NAME, MoreExecutors.directExecutor(), messageQueue::add);
        String code =
                String.format(
                        """
                        (async () => {
                            const port = await android.getNamedPort('%s');
                            port.postMessage(new ArrayBuffer(1024));
                            return 'PASS';
                        })()
                        """,
                        PORT_NAME);
        ListenableFuture<String> result = mJsIsolate.evaluateJavaScriptAsync(code);
        Assert.assertEquals("PASS", result.get(TIMEOUT_SECONDS, TimeUnit.SECONDS));
        // More thorough tests for arraybuffer content exist in AndroidX. We're just casually
        // testing that an ArrayBuffer message was sent.
        Assert.assertTrue(
                Arrays.equals(
                        new byte[1024],
                        messageQueue.poll(TIMEOUT_SECONDS, TimeUnit.SECONDS).getArrayBuffer()));
    }

    private void assertPostMessageThrowsTypeError(String expression) throws Throwable {
        mJsIsolate.provideMessagePort(
                PORT_NAME, MoreExecutors.directExecutor(), EXPECT_NO_INCOMING_MESSAGES);
        String code =
                String.format(
                        """
                        (async () => {
                            const port = await android.getNamedPort('%s');
                            try {
                                port.postMessage(%s);
                            } catch (error) {
                                if (error instanceof TypeError) {
                                    return 'PASS';
                                }
                                throw 'Fail: Expected TypeError but got ' + error;
                            }
                            throw 'Fail: postMessage did not throw';
                        })()
                        """,
                        PORT_NAME, expression);
        ListenableFuture<String> result = mJsIsolate.evaluateJavaScriptAsync(code);
        Assert.assertEquals("PASS", result.get(TIMEOUT_SECONDS, TimeUnit.SECONDS));
    }

    @Test
    public void testPostMessageWithArgs_none_throwsTypeError() throws Throwable {
        assertPostMessageThrowsTypeError("");
    }

    @Test
    public void testPostMessageWithArgs_multiple_throwsTypeError() throws Throwable {
        assertPostMessageThrowsTypeError("'one', 'two'");
    }

    @Test
    public void testPostMessageWithArgs_null_throwsTypeError() throws Throwable {
        assertPostMessageThrowsTypeError("null");
    }

    @Test
    public void testPostMessageWithArgs_undefined_throwsTypeError() throws Throwable {
        assertPostMessageThrowsTypeError("undefined");
    }

    @Test
    public void testPostMessageWithArgs_number_throwsTypeError() throws Throwable {
        assertPostMessageThrowsTypeError("42");
    }

    @Test
    public void testPostMessageWithArgs_bigInt_throwsTypeError() throws Throwable {
        assertPostMessageThrowsTypeError("42n");
    }

    @Test
    public void testPostMessageWithArgs_bool_throwsTypeError() throws Throwable {
        assertPostMessageThrowsTypeError("false");
    }

    @Test
    public void testPostMessageWithArgs_symbol_throwsTypeError() throws Throwable {
        assertPostMessageThrowsTypeError("Symbol('id')");
    }

    @Test
    public void testPostMessageWithArgs_plainObject_throwsTypeError() throws Throwable {
        assertPostMessageThrowsTypeError("{}");
    }

    @Test
    public void testPostMessageWithArgs_function_throwsTypeError() throws Throwable {
        assertPostMessageThrowsTypeError("() => {}");
    }

    // The only array type currently accepted is strictly ArrayBuffer. There MUST NOT be any
    // automatic conversion to ArrayBuffer from other types - even Uint8Array or Int8Array.
    @Test
    public void testPostMessageWithArgs_plainArray_throwsTypeError() throws Throwable {
        assertPostMessageThrowsTypeError("[]");
    }

    @Test
    public void testPostMessageWithArgs_uint8Array_throwsTypeError() throws Throwable {
        assertPostMessageThrowsTypeError("new Uint8Array(1024)");
    }

    @Test
    public void testPostMessageWithArgs_int8Array_throwsTypeError() throws Throwable {
        assertPostMessageThrowsTypeError("new Int8Array(1024)");
    }

    @Test
    public void testPostMessageWithArgs_uint16Array_throwsTypeError() throws Throwable {
        assertPostMessageThrowsTypeError("new Uint16Array(1024)");
    }

    @Test
    public void testPostMessageWithArgs_int16Array_throwsTypeError() throws Throwable {
        assertPostMessageThrowsTypeError("new Int16Array(1024)");
    }

    @Test
    public void testPostMessageWithArgs_uint32Array_throwsTypeError() throws Throwable {
        assertPostMessageThrowsTypeError("new Uint32Array(1024)");
    }

    @Test
    public void testPostMessageWithArgs_int32Array_throwsTypeError() throws Throwable {
        assertPostMessageThrowsTypeError("new Int32Array(1024)");
    }

    @Test
    public void testPostMessageWithArgs_bigInt64Array_throwsTypeError() throws Throwable {
        assertPostMessageThrowsTypeError("new BigInt64Array(1024)");
    }

    @Test
    public void testPostMessageWithArgs_bigUint64Array_throwsTypeError() throws Throwable {
        assertPostMessageThrowsTypeError("new BigUint64Array(1024)");
    }

    @Test
    public void testPostMessageWithArgs_float32Array_throwsTypeError() throws Throwable {
        assertPostMessageThrowsTypeError("new Float32Array(1024)");
    }

    @Test
    public void testPostMessageWithArgs_float64Array_throwsTypeError() throws Throwable {
        assertPostMessageThrowsTypeError("new Float64Array(1024)");
    }

    private void assertGetNamedPortThrowsSyncTypeError(String expression) throws Throwable {
        String code =
                String.format(
                        """
                        (async () => {
                            // Throws should be asynchronous
                            let promise;
                            try {
                                promise = android.getNamedPort(%s);
                            } catch (error) {
                                if (error instanceof TypeError) {
                                    return 'PASS';
                                }
                                throw 'Fail: Expected synchronous TypeError but got synchronous '
                                        + error;
                            }
                            try {
                                await promise;
                            } catch (error) {
                                throw 'Fail: Expected synchronous TypeError but got asynchronous '
                                        + error;
                            }
                            throw 'Fail: getNamedPort did not throw (neither sync nor async)';
                        })()
                        """,
                        expression);
        ListenableFuture<String> result = mJsIsolate.evaluateJavaScriptAsync(code);
        Assert.assertEquals("PASS", result.get(TIMEOUT_SECONDS, TimeUnit.SECONDS));
    }

    @Test
    public void testGetNamedPortWithArgs_none_throwsTypeError() throws Throwable {
        assertGetNamedPortThrowsSyncTypeError("");
    }

    @Test
    public void testGetNamedPortWithArgs_multiple_throwsTypeError() throws Throwable {
        assertGetNamedPortThrowsSyncTypeError("'one', 'two'");
    }

    @Test
    public void testGetNamedPortWithArgs_null_throwsTypeError() throws Throwable {
        assertGetNamedPortThrowsSyncTypeError("null");
    }

    @Test
    public void testGetNamedPortWithArgs_undefined_throwsTypeError() throws Throwable {
        assertGetNamedPortThrowsSyncTypeError("undefined");
    }

    @Test
    public void testGetNamedPortWithArgs_number_throwsTypeError() throws Throwable {
        assertGetNamedPortThrowsSyncTypeError("42");
    }

    @Test
    public void testGetNamedPort_beforeProvidePort_yieldsAndAwaits() throws Throwable {
        String code =
                String.format(
                        """
                        (async () => {
                            const port = await android.getNamedPort('%s');
                            port.onmessage = (event) => port.postMessage(event.data);
                        })();
                        """,
                        PORT_NAME);
        ListenableFuture<String> earlyEval = mJsIsolate.evaluateJavaScriptAsync(code);
        // earlyEval won't resolve until we provide a port, so use an evaluation to synchronize
        // on the code being executed.
        mJsIsolate
                .evaluateJavaScriptAsync("'Eval used for synchronization'")
                .get(TIMEOUT_SECONDS, TimeUnit.SECONDS);
        Assert.assertFalse(earlyEval.isDone());

        String messageString = "Hello World";
        final BlockingQueue<Message> messageQueue = new LinkedBlockingQueue<>();
        MessagePort messagePort =
                mJsIsolate.provideMessagePort(
                        PORT_NAME, MoreExecutors.directExecutor(), messageQueue::add);

        earlyEval.get(TIMEOUT_SECONDS, TimeUnit.SECONDS);

        // Make sure the port works as expected.
        messagePort.postMessage(Message.createString(messageString));
        Assert.assertEquals(
                messageString, messageQueue.poll(TIMEOUT_SECONDS, TimeUnit.SECONDS).getString());
    }

    @Test
    public void testGetNamedPort_multipleTimes_returnsConsistentPort() throws Throwable {
        String code =
                String.format(
                        """
                        const portName = '%s';
                        (async () => {
                            // Before provide port
                            const port1promise = android.getNamedPort(portName);
                            const port2promise = android.getNamedPort(portName);

                            // Yield and wait for port provision.
                            const port1 = await port1promise;
                            port1.onmessage = (event) => port1.postMessage(
                                    'This is not the message you are looking for.');
                            // After provide port, but resolving a second promise.
                            const port2 = await port2promise;
                            port2.onmessage = (event) => port2.postMessage(
                                    '... or this...');
                            // After provide port, should resolve immediately
                            const port3 = await android.getNamedPort(portName);
                            port3.onmessage = (event) => port3.postMessage(
                                    '... not even this...');
                            // After provide port, should resolve immediately (again)
                            const port4 = await android.getNamedPort(portName);
                            port4.onmessage = (event) => port4.postMessage(event.data);

                            // Getting the port again should return the same object, and you should
                            // be able to change the handler. (The instrumentation thread will not
                            // post a message before this entire promise resolves, so the old
                            // handler should never run.)
                            if (port1 !== port2 || port1 !== port3 || port1 !== port4) {
                                throw 'getNamedPort did not return the same port object';
                            }

                            return 'PASS';
                        })()
                        """,
                        PORT_NAME);
        ListenableFuture<String> setupFuture = mJsIsolate.evaluateJavaScriptAsync(code);
        // setupFuture won't resolve until we provide a port, so use an evaluation to synchronize on
        // the code being executed.
        mJsIsolate
                .evaluateJavaScriptAsync("'Eval used for synchronization'")
                .get(TIMEOUT_SECONDS, TimeUnit.SECONDS);
        Assert.assertFalse(setupFuture.isDone());
        String messageString = "Hello World";
        final BlockingQueue<Message> messageQueue = new LinkedBlockingQueue<>();
        MessagePort messagePort =
                mJsIsolate.provideMessagePort(
                        PORT_NAME, MoreExecutors.directExecutor(), messageQueue::add);
        Assert.assertEquals("PASS", setupFuture.get(TIMEOUT_SECONDS, TimeUnit.SECONDS));

        // Post twice so that we're slightly more confident that the replaced onmessage handlers
        // never run, in case they're actually run in an (un)lucky order.
        messagePort.postMessage(Message.createString(messageString));
        Assert.assertEquals(
                messageString, messageQueue.poll(TIMEOUT_SECONDS, TimeUnit.SECONDS).getString());
        messagePort.postMessage(Message.createString(messageString));
        Assert.assertEquals(
                messageString, messageQueue.poll(TIMEOUT_SECONDS, TimeUnit.SECONDS).getString());
    }

    /** Verifies that a closed port can still be retrieved. */
    @Test
    public void testGetNamedPort_closedPort_returnsConsistentPort() throws Throwable {
        String portNameA = "TestPortA";
        String portNameB = "TestPortB";
        MessagePort messagePortA =
                mJsIsolate.provideMessagePort(
                        portNameA, MoreExecutors.directExecutor(), EXPECT_NO_INCOMING_MESSAGES);
        messagePortA.close();
        mJsIsolate.provideMessagePort(
                portNameB, MoreExecutors.directExecutor(), EXPECT_NO_INCOMING_MESSAGES);
        String code =
                String.format(
                        """
                        const portNameA = '%s';
                        const portNameB = '%s';
                        (async () => {
                            // Closed by remote
                            const portA = await android.getNamedPort(portNameA);
                            portA.postMessage('silently dropped');

                            // Closed by local
                            const portB1 = await android.getNamedPort(portNameB);
                            portB1.close();
                            portB1.postMessage('silently dropped');
                            const portB2 = await android.getNamedPort(portNameB);
                            portB2.postMessage('silently dropped');
                            if (portB1 !== portB2) {
                                throw 'portB1 is not equal to portB2';
                            }

                            return 'PASS';
                        })()
                        """,
                        portNameA, portNameB);
        ListenableFuture<String> setupFuture = mJsIsolate.evaluateJavaScriptAsync(code);
        Assert.assertEquals("PASS", setupFuture.get(TIMEOUT_SECONDS, TimeUnit.SECONDS));
    }

    @Test
    public void testOnMessageInitiallyNull() throws Throwable {
        String code =
                String.format(
                        """
                        (async () => {
                            const port = await android.getNamedPort('%s');
                            const onmessage = port.onmessage;
                            if (onmessage !== null) {
                                throw 'Fail: Expected onmessage to be null, but was '
                                        + onmessage;
                            }
                            return 'PASS';
                        })()
                        """,
                        PORT_NAME);

        mJsIsolate.provideMessagePort(
                PORT_NAME, MoreExecutors.directExecutor(), EXPECT_NO_INCOMING_MESSAGES);

        ListenableFuture<String> setupFuture = mJsIsolate.evaluateJavaScriptAsync(code);
        String result = setupFuture.get(TIMEOUT_SECONDS, TimeUnit.SECONDS);
        Assert.assertEquals("PASS", result);
    }

    private void assertInvalidOnMessageHandlerConvertsToNullAndDropsMessages(String expression)
            throws Throwable {
        MessagePort port =
                mJsIsolate.provideMessagePort(
                        PORT_NAME, MoreExecutors.directExecutor(), EXPECT_NO_INCOMING_MESSAGES);
        // Consumed by invalid message handler. Note that this should be internally queued until
        // the first time onmessage is set (even if to an invalid handler).
        port.postMessage(Message.createString("dropped message"));
        String code1 =
                String.format(
                        """
                        let port;
                        (async () => {
                            port = await android.getNamedPort('%s');
                            port.onmessage = %s;
                            if (port.onmessage !== null) {
                                throw 'Expected null onmessage handler when set to non-function.';
                            }
                            return 'NEXT';
                        })()
                        """,
                        PORT_NAME, expression);
        ListenableFuture<String> result1 = mJsIsolate.evaluateJavaScriptAsync(code1);
        Assert.assertEquals("NEXT", result1.get(TIMEOUT_SECONDS, TimeUnit.SECONDS));

        String code2 =
                """
                new Promise((resolve, reject) => {
                    port.onmessage = (event) => {resolve(event.data)};
                    if (port.onmessage === null) {
                        reject('Expected non-null onmessage handler when set to function.');
                    }
                })
                """;
        ListenableFuture<String> result2 = mJsIsolate.evaluateJavaScriptAsync(code2);
        mJsIsolate
                .evaluateJavaScriptAsync("'Eval used for synchronization'")
                .get(TIMEOUT_SECONDS, TimeUnit.SECONDS);
        // Consumed by valid message handler.
        port.postMessage(Message.createString("processed message"));
        Assert.assertEquals("processed message", result2.get(TIMEOUT_SECONDS, TimeUnit.SECONDS));
    }

    @Test
    public void testSetOnMessageHandler_null_nullifiesHandlerAndDropsMessages() throws Throwable {
        assertInvalidOnMessageHandlerConvertsToNullAndDropsMessages("null");
    }

    @Test
    public void testSetOnMessageHandler_undefined_nullifiesHandlerAndDropsMessages()
            throws Throwable {
        assertInvalidOnMessageHandlerConvertsToNullAndDropsMessages("undefined");
    }

    @Test
    public void testSetOnMessageHandler_string_nullifiesHandlerAndDropsMessages() throws Throwable {
        assertInvalidOnMessageHandlerConvertsToNullAndDropsMessages("'()=>{}'");
    }

    @Test
    public void testSetOnMessageHandler_plainObject_nullifiesHandlerAndDropsMessages()
            throws Throwable {
        assertInvalidOnMessageHandlerConvertsToNullAndDropsMessages("{}");
    }

    private void assertPostingMessagesToIsolateExceedsMemoryLimit(
            Message message, int count, boolean busyIsolate) throws Throwable {
        MessagePort messagePort =
                mJsIsolate.provideMessagePort(
                        PORT_NAME, MoreExecutors.directExecutor(), EXPECT_NO_INCOMING_MESSAGES);
        String code =
                String.format(
                        """
                        (async () => {
                            const port = await android.getNamedPort('%s');
                            port.onmessage = (event)=>{};
                        })()
                        """,
                        PORT_NAME);
        mJsIsolate.evaluateJavaScriptAsync(code).get(TIMEOUT_SECONDS, TimeUnit.SECONDS);
        if (busyIsolate) {
            // Ensure the isolate thread cannot process any messages.
            mJsIsolate.evaluateJavaScriptAsync("while(true);");
        }

        for (int i = 0; i < count; i++) {
            messagePort.postMessage(message);
        }

        final CountDownLatch sandboxCrashedLatch = new CountDownLatch(1);
        mJsIsolate.addOnTerminatedCallback(
                MoreExecutors.directExecutor(),
                info -> {
                    if (info.getStatus() != TerminationInfo.STATUS_MEMORY_LIMIT_EXCEEDED) {
                        Assert.fail(
                                "Expected: STATUS_MEMORY_LIMIT_EXCEEDED but was: "
                                        + info.getStatusString());
                    }
                    sandboxCrashedLatch.countDown();
                });
        Assert.assertTrue(sandboxCrashedLatch.await(TIMEOUT_SECONDS, TimeUnit.SECONDS));
    }

    @Test
    public void testExceedMemoryLimit_emptyStrings_sandboxDead() throws Throwable {
        // Memory accounting should ensure very small messages (e.g. 0 bytes) count as at least a
        // whole page in size. Note that a page size of 4096 isn't an API guarantee. It could be
        // more, but isn't currently likely to be less. If it's less than 4096, this test may fail!
        final int messageCount = MAX_HEAP_SIZE / 4096 + 1;
        assertPostingMessagesToIsolateExceedsMemoryLimit(
                Message.createString(""), messageCount, true);
    }

    @Test
    public void testExceedMemoryLimit_emptyArrayBuffers_sandboxDead() throws Throwable {
        // Memory accounting should ensure very small messages (e.g. 0 bytes) count as at least a
        // whole page in size. Note that a page size of 4096 isn't an API guarantee. It could be
        // more, but isn't currently likely to be less. If it's less than 4096, this test may fail!
        final int messageCount = MAX_HEAP_SIZE / 4096 + 1;
        assertPostingMessagesToIsolateExceedsMemoryLimit(
                Message.createArrayBuffer(new byte[0]), messageCount, true);
    }

    @Test
    public void testExceedMemoryLimit_smallStrings_sandboxDead() throws Throwable {
        // Size in UTF-16 bytes
        final int messageSize = 8192;
        final int messageCount = MAX_HEAP_SIZE / messageSize + 1;
        assertPostingMessagesToIsolateExceedsMemoryLimit(
                Message.createString("a".repeat(messageSize / 2)), messageCount, true);
    }

    @Test
    public void testExceedMemoryLimit_smallArrayBuffers_sandboxDead() throws Throwable {
        final int messageSize = 8192;
        final int messageCount = MAX_HEAP_SIZE / messageSize + 1;
        assertPostingMessagesToIsolateExceedsMemoryLimit(
                Message.createArrayBuffer(new byte[messageSize]), messageCount, true);
    }

    @Test
    public void testExceedMemoryLimit_largeStrings_sandboxDead() throws Throwable {
        // Size in UTF-16 bytes
        final int messageSize = 1048576;
        final int messageCount = MAX_HEAP_SIZE / messageSize + 1;
        assertPostingMessagesToIsolateExceedsMemoryLimit(
                Message.createString("a".repeat(messageSize / 2)), messageCount, true);
    }

    @Test
    public void testExceedMemoryLimit_largeArrayBuffers_sandboxDead() throws Throwable {
        final int messageSize = 1048576;
        final int messageCount = MAX_HEAP_SIZE / messageSize + 1;
        assertPostingMessagesToIsolateExceedsMemoryLimit(
                Message.createArrayBuffer(new byte[messageSize]), messageCount, true);
    }

    @Test
    public void testExceedMemoryLimit_singleOversizedString_sandboxDead() throws Throwable {
        // For single oversized messages there is no need to busy-loop the isolate.
        // So, ensure handling still works without active evaluations, in contrast to other tests.
        assertPostingMessagesToIsolateExceedsMemoryLimit(
                Message.createString("a".repeat(MAX_HEAP_SIZE + 1)), 1, false);
    }

    @Test
    public void testExceedMemoryLimit_singleOversizedArrayBuffer_sandboxDead() throws Throwable {
        // For single oversized messages there is no need to busy-loop the isolate.
        // So, ensure handling still works without active evaluations, in contrast to other tests.
        assertPostingMessagesToIsolateExceedsMemoryLimit(
                Message.createArrayBuffer(new byte[MAX_HEAP_SIZE + 1]), 1, false);
    }

    private void assertMessageMemoryRecycled(Message message, int messageCount, int batchSize)
            throws Throwable {
        MessagePort messagePort =
                mJsIsolate.provideMessagePort(
                        PORT_NAME, MoreExecutors.directExecutor(), EXPECT_NO_INCOMING_MESSAGES);
        String code =
                String.format(
                        """
                        let remainingMessages = 0;
                        let waiting = null;
                        function setRemainingMessages(count) {
                            if (remainingMessages !== 0) {
                                throw 'remainingMessages is not zero';
                            }
                            remainingMessages = count;
                        }
                        function waitForMessages() {
                            if (waiting !== null) {
                                throw 'already waiting';
                            }
                            return new Promise((resolve, reject) => {waiting = resolve;});
                        }
                        function countMessage(ignored_event) {
                            remainingMessages--;
                            if (remainingMessages === 0) {
                                waiting('DONE');
                                waiting = null;
                            } else if (remainingMessages < 0) {
                                // This will probably never get surfaced if it happens.
                                throw 'counted unexpected message';
                            }
                        }
                        (async () => {
                            const port = await android.getNamedPort('%s');
                            port.onmessage = countMessage;
                        })()
                        """,
                        PORT_NAME);
        mJsIsolate.evaluateJavaScriptAsync(code).get(TIMEOUT_SECONDS, TimeUnit.SECONDS);

        // Batch multiple messages at a time. Checkpoint using waitForMessages to ensure the GC gets
        // a chance to run. This may post a few extra messages than messageCount says to, but that's
        // fine for these tests.
        for (int messageNum = 0; messageNum < messageCount; messageNum += batchSize) {
            mJsIsolate
                    .evaluateJavaScriptAsync("setRemainingMessages(" + batchSize + ")")
                    .get(TIMEOUT_SECONDS, TimeUnit.SECONDS);
            ListenableFuture<String> resultFuture =
                    mJsIsolate.evaluateJavaScriptAsync("waitForMessages()");
            for (int i = 0; i < batchSize; i++) {
                messagePort.postMessage(message);
            }
            String result = resultFuture.get(TIMEOUT_SECONDS, TimeUnit.SECONDS);
            Assert.assertEquals("DONE", result);
        }
    }

    @Test
    public void testProcessedMessages_emptyStrings_memoryRecycled() throws Throwable {
        final int messageCount = 2 * MAX_HEAP_SIZE / 4096 + 1;
        final int batchSize = 1048576 / 4096;
        assertMessageMemoryRecycled(Message.createString(""), messageCount, batchSize);
    }

    @Test
    public void testProcessedMessages_emptyArrayBuffers_memoryRecycled() throws Throwable {
        final int messageCount = 2 * MAX_HEAP_SIZE / 4096 + 1;
        final int batchSize = 1048576 / 4096;
        assertMessageMemoryRecycled(
                Message.createArrayBuffer(new byte[0]), messageCount, batchSize);
    }

    @Test
    public void testProcessedMessages_smallStrings_memoryRecycled() throws Throwable {
        // Size in UTF-16 bytes
        final int messageSize = 8192;
        final int messageCount = 2 * MAX_HEAP_SIZE / messageSize;
        final int batchSize = 1048576 / messageSize;
        assertMessageMemoryRecycled(
                Message.createString("a".repeat(messageSize / 2)), messageCount, batchSize);
    }

    @Test
    public void testProcessedMessages_smallArrayBuffers_memoryRecycled() throws Throwable {
        final int messageSize = 8192;
        final int messageCount = 2 * MAX_HEAP_SIZE / messageSize;
        final int batchSize = 1048576 / messageSize;
        assertMessageMemoryRecycled(
                Message.createArrayBuffer(new byte[messageSize]), messageCount, batchSize);
    }

    @Test
    public void testProcessedMessages_largeStrings_memoryRecycled() throws Throwable {
        // Size in UTF-16 bytes
        final int messageSize = 1048576;
        final int messageCount = 2 * MAX_HEAP_SIZE / messageSize;
        final int batchSize = 1048576 / messageSize;
        assertMessageMemoryRecycled(
                Message.createString("a".repeat(messageSize / 2)), messageCount, batchSize);
    }

    @Test
    public void testProcessedMessages_largeArrayBuffers_memoryRecycled() throws Throwable {
        final int messageSize = 1048576;
        final int messageCount = 2 * MAX_HEAP_SIZE / messageSize;
        final int batchSize = 1048576 / messageSize;
        assertMessageMemoryRecycled(
                Message.createArrayBuffer(new byte[messageSize]), messageCount, batchSize);
    }
}
