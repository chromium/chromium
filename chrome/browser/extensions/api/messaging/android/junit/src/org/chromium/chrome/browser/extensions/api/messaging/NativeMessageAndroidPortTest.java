// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.extensions.api.messaging;

import android.content.ComponentName;
import android.content.Context;
import android.content.ContextWrapper;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.Bundle;
import android.os.DeadObjectException;
import android.os.IBinder;
import android.os.RemoteException;
import android.os.TransactionTooLargeException;

import androidx.annotation.Nullable;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.profiles.Profile;

import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeUnit;

/** Unit tests for {@link NativeMessageAndroidPort}. */
@RunWith(BaseRobolectricTestRunner.class)
public class NativeMessageAndroidPortTest {
    private static final String TARGET_PACKAGE = "com.example.extensionreceiver";
    private static final String EXTENSION_ID = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
    private static final byte[][] NO_CERTIFICATES = new byte[0][];

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile;
    @Mock private IExtensionNativeMessageService mMockExtensionService;
    // Mocks the C++ bridge for NativeMessagingManager to prevent an UnsatisfiedLinkError.
    @Mock private NativeMessagingManager.Natives mNativeMessagingManagerJni;

    private TestContext mTestContext;
    private IBrowserNativeMessageService mFakeBrowserService;

    private static class TestContext extends ContextWrapper {
        private ServiceConnection mServiceConnection;

        public TestContext(Context base) {
            super(base);
        }

        @Override
        public boolean bindService(Intent service, ServiceConnection conn, int flags) {
            mServiceConnection = conn;
            return true;
        }

        public void triggerServiceConnected(IBinder service) {
            mServiceConnection.onServiceConnected(
                    new ComponentName(TARGET_PACKAGE, "NativeMessageService"), service);
        }
    }

    private static class FakeNativeMessagePort extends IExtensionNativeMessagePort.Stub {
        public final List<String> receivedMessages = new ArrayList<>();
        public final @Nullable IExtensionNativeMessageCallback callback;
        public boolean isDisconnected;
        public boolean shouldThrowOnPostMessage;
        public boolean shouldThrowTransactionTooLarge;
        public int failAfterMessageCount = -1;

        public FakeNativeMessagePort(@Nullable IExtensionNativeMessageCallback callback) {
            this.callback = callback;
        }

        @Override
        public void postMessage(MessagePayload payload, Bundle extras) throws RemoteException {
            if (shouldThrowTransactionTooLarge) {
                throw new TransactionTooLargeException("Payload too large.");
            }
            if (shouldThrowOnPostMessage || receivedMessages.size() == failAfterMessageCount) {
                throw new DeadObjectException("Target process is dead.");
            }
            byte[] bytes = payload.getInlineBytes();
            if (bytes != null) {
                receivedMessages.add(new String(bytes, StandardCharsets.UTF_8));
            }
        }

        @Override
        public void disconnect() {
            isDisconnected = true;
        }
    }

    private static MessagePayload createPayload(String message) {
        MessagePayload payload = new MessagePayload();
        payload.setInlineBytes(message.getBytes(StandardCharsets.UTF_8));
        return payload;
    }

    private static class TestPortObserver
            implements NativeMessageAndroidPort.Observer, NativeMessageAndroidPort.TestObserver {
        public final List<String> receivedMessages = new ArrayList<>();
        public @Nullable String closedError;
        public boolean isPortDestroyed;

        @Override
        public void onPortDestroying(NativeMessageAndroidPort port) {
            isPortDestroyed = true;
        }

        @Override
        public void onMessageFromApp(String message) {
            receivedMessages.add(message);
        }

        @Override
        public void onChannelClosed(String errorMessage) {
            closedError = errorMessage;
        }
    }

    @Before
    public void setUp() {
        NativeMessagingManagerJni.setInstanceForTesting(mNativeMessagingManagerJni);
        Mockito.when(mProfile.getOriginalProfile()).thenReturn(mProfile);
        mTestContext = new TestContext(RuntimeEnvironment.application);
        ContextUtils.initApplicationContextForTests(mTestContext);

        mFakeBrowserService =
                new IBrowserNativeMessageService.Stub() {
                    @Override
                    public void connectExtension(
                            String extensionId, Bundle info, IConnectExtensionCallback callback)
                            throws RemoteException {
                        if (EXTENSION_ID.equals(extensionId)) {
                            callback.onSuccess(mMockExtensionService);
                        } else {
                            callback.onError("Unknown extension");
                        }
                    }
                };
    }

    @After
    public void tearDown() {
        NativeMessagingManager.getForProfile(mProfile).destroy();
    }

    private @Nullable String connectToApp(NativeMessageAndroidPort port) {
        return port.connectToApp(
                mProfile,
                TARGET_PACKAGE,
                EXTENSION_ID,
                /* isVerifiedExtension= */ true,
                NO_CERTIFICATES);
    }

    // Test that messages queued in NativeMessageAndroidPort before the app connects
    // are flushed in FIFO order once connection is established, and bidirectional
    // replies from the app are delivered.
    @Test
    public void testConnectToAppAndAddPort() throws Exception {
        List<FakeNativeMessagePort> createdPorts = new ArrayList<>();
        Mockito.doAnswer(
                        invocation -> {
                            IExtensionNativeMessageCallback messageReceiver =
                                    invocation.getArgument(0);
                            IConnectPortCallback callback = invocation.getArgument(1);
                            FakeNativeMessagePort fakePort =
                                    new FakeNativeMessagePort(messageReceiver);
                            createdPorts.add(fakePort);
                            callback.onSuccess(fakePort);
                            return null;
                        })
                .when(mMockExtensionService)
                .connectPort(Mockito.any(), Mockito.any());

        NativeMessageAndroidPort port = new NativeMessageAndroidPort();
        TestPortObserver portObserver = new TestPortObserver();
        port.setTestObserver(portObserver);

        var successWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Extensions.NativeMessaging.Android.SentMessageSize.Success",
                                "msg_1".getBytes(StandardCharsets.UTF_8).length)
                        .expectIntRecord(
                                "Extensions.NativeMessaging.Android.SentMessageSize.Success",
                                "msg_2".getBytes(StandardCharsets.UTF_8).length)
                        .expectNoRecords(
                                "Extensions.NativeMessaging.Android.SentMessageSize.TooLarge")
                        .build();

        // 1. Queue messages before service is connected.
        port.forwardMessageToApp("msg_1");
        port.forwardMessageToApp("msg_2");

        Assert.assertNull(connectToApp(port));

        // 2. Service connects and completes authentication.
        mTestContext.triggerServiceConnected(mFakeBrowserService.asBinder());
        RobolectricUtil.runAllBackgroundAndUi();

        successWatcher.assertExpected();

        // 3. Verify port connected and flushed pending messages in FIFO order.
        Assert.assertEquals(1, createdPorts.size());
        Assert.assertEquals(List.of("msg_1", "msg_2"), createdPorts.get(0).receivedMessages);

        // 4. Test bidirectional reply from the app.
        createdPorts.get(0).callback.onMessage(createPayload("reply_from_app"), new Bundle());
        RobolectricUtil.runAllBackgroundAndUi();
        Assert.assertEquals(List.of("reply_from_app"), portObserver.receivedMessages);
    }

    // Test adding a second port to an already-connected extension session, verifying
    // it connects immediately without re-authenticating and maintains isolated messaging.
    @Test
    public void testAddPortToConnectedApp() throws Exception {
        List<FakeNativeMessagePort> createdPorts = new ArrayList<>();
        Mockito.doAnswer(
                        invocation -> {
                            IExtensionNativeMessageCallback messageReceiver =
                                    invocation.getArgument(0);
                            IConnectPortCallback callback = invocation.getArgument(1);
                            FakeNativeMessagePort fakePort =
                                    new FakeNativeMessagePort(messageReceiver);
                            createdPorts.add(fakePort);
                            callback.onSuccess(fakePort);
                            return null;
                        })
                .when(mMockExtensionService)
                .connectPort(Mockito.any(), Mockito.any());

        // 1. Establish session first with port 1.
        NativeMessageAndroidPort port1 = new NativeMessageAndroidPort();
        Assert.assertNull(connectToApp(port1));
        mTestContext.triggerServiceConnected(mFakeBrowserService.asBinder());
        RobolectricUtil.runAllBackgroundAndUi();

        Assert.assertEquals(1, createdPorts.size());

        // 2. Add port 2 to the already-connected session.
        NativeMessageAndroidPort port2 = new NativeMessageAndroidPort();
        TestPortObserver port2Observer = new TestPortObserver();
        port2.setTestObserver(port2Observer);

        Assert.assertNull(connectToApp(port2));
        RobolectricUtil.runAllBackgroundAndUi();

        Assert.assertEquals(2, createdPorts.size());

        // 3. Port 2 sends a message directly.
        port2.forwardMessageToApp("msg_port2");
        Assert.assertEquals(List.of("msg_port2"), createdPorts.get(1).receivedMessages);
        Assert.assertTrue(createdPorts.get(0).receivedMessages.isEmpty());

        // 4. App replies specifically to port 2.
        createdPorts.get(1).callback.onMessage(createPayload("reply_to_port2"), new Bundle());
        RobolectricUtil.runAllBackgroundAndUi();
        Assert.assertEquals(List.of("reply_to_port2"), port2Observer.receivedMessages);
    }

    // Test that if the target app returns null from connectPort, the port is removed
    // from active ports and closed with an error.
    @Test
    public void testAppRejectsPortCreation() throws Exception {
        Mockito.doAnswer(
                        invocation -> {
                            IConnectPortCallback callback = invocation.getArgument(1);
                            callback.onError("App rejected port creation");
                            return null;
                        })
                .when(mMockExtensionService)
                .connectPort(Mockito.any(), Mockito.any());

        NativeMessageAndroidPort port = new NativeMessageAndroidPort();
        TestPortObserver portObserver = new TestPortObserver();
        port.setTestObserver(portObserver);

        Assert.assertNull(connectToApp(port));
        mTestContext.triggerServiceConnected(mFakeBrowserService.asBinder());
        RobolectricUtil.runAllBackgroundAndUi();

        Assert.assertEquals(
                "Could not connect port to " + TARGET_PACKAGE + ".", portObserver.closedError);
    }

    // Test that if a port is destroyed while connectPort is in flight,
    // onConnectPortSuccess detects the cancellation and immediately disconnects the remote stub.
    @Test
    public void testPortDestroyedWhileConnectingCancelsRemotePort() throws Exception {
        FakeNativeMessagePort fakeAppPort = new FakeNativeMessagePort(null);
        Mockito.doAnswer(
                        invocation -> {
                            IConnectPortCallback callback = invocation.getArgument(1);
                            callback.onSuccess(fakeAppPort);
                            return null;
                        })
                .when(mMockExtensionService)
                .connectPort(Mockito.any(), Mockito.any());

        // 1. Establish and authenticate session first.
        NativeMessageAndroidPort setupPort = new NativeMessageAndroidPort();
        Assert.assertNull(connectToApp(setupPort));
        mTestContext.triggerServiceConnected(mFakeBrowserService.asBinder());
        RobolectricUtil.runAllBackgroundAndUi();

        // 2. Add new port to the connected session (initiates connectPort).
        NativeMessageAndroidPort port = new NativeMessageAndroidPort();
        Assert.assertNull(connectToApp(port));

        // 3. Destroy port while connectPort callback is in-flight before tasks run.
        port.destroy();

        RobolectricUtil.runAllBackgroundAndUi();

        // 4. The remote stub should have been told to disconnect immediately upon connect.
        Assert.assertTrue(fakeAppPort.isDisconnected);
    }

    // Test that when the companion app disconnects a port via callback.onDisconnect(),
    // the channel is closed, the port is notified, and no redundant disconnect()
    // is sent to the app.
    @Test
    public void testAppDisconnectsPort() throws Exception {
        List<FakeNativeMessagePort> createdPorts = new ArrayList<>();
        Mockito.doAnswer(
                        invocation -> {
                            IExtensionNativeMessageCallback messageReceiver =
                                    invocation.getArgument(0);
                            IConnectPortCallback callback = invocation.getArgument(1);
                            FakeNativeMessagePort fakePort =
                                    new FakeNativeMessagePort(messageReceiver);
                            createdPorts.add(fakePort);
                            callback.onSuccess(fakePort);
                            return null;
                        })
                .when(mMockExtensionService)
                .connectPort(Mockito.any(), Mockito.any());

        NativeMessageAndroidPort port = new NativeMessageAndroidPort();
        TestPortObserver portObserver = new TestPortObserver();
        port.setTestObserver(portObserver);

        Assert.assertNull(connectToApp(port));
        mTestContext.triggerServiceConnected(mFakeBrowserService.asBinder());
        RobolectricUtil.runAllBackgroundAndUi();

        Assert.assertEquals(1, createdPorts.size());
        FakeNativeMessagePort fakeAppPort = createdPorts.get(0);

        // App disconnects the port.
        fakeAppPort.callback.onDisconnect();
        RobolectricUtil.runAllBackgroundAndUi();

        // Channel should be closed with empty error string (app-initiated disconnect).
        Assert.assertEquals("", portObserver.closedError);
        // Ensure Chrome did not ping the app back with disconnect().
        Assert.assertFalse(fakeAppPort.isDisconnected);
    }

    // Test that when Chrome/browser destroys an active port (e.g. extension closes port),
    // it sends a disconnect() signal to the companion app and cleans up state.
    @Test
    public void testBrowserDisconnectsPort() throws Exception {
        List<FakeNativeMessagePort> createdPorts = new ArrayList<>();
        Mockito.doAnswer(
                        invocation -> {
                            IExtensionNativeMessageCallback messageReceiver =
                                    invocation.getArgument(0);
                            IConnectPortCallback callback = invocation.getArgument(1);
                            FakeNativeMessagePort fakePort =
                                    new FakeNativeMessagePort(messageReceiver);
                            createdPorts.add(fakePort);
                            callback.onSuccess(fakePort);
                            return null;
                        })
                .when(mMockExtensionService)
                .connectPort(Mockito.any(), Mockito.any());

        NativeMessageAndroidPort port = new NativeMessageAndroidPort();
        Assert.assertNull(connectToApp(port));
        mTestContext.triggerServiceConnected(mFakeBrowserService.asBinder());
        RobolectricUtil.runAllBackgroundAndUi();

        Assert.assertEquals(1, createdPorts.size());
        FakeNativeMessagePort fakeAppPort = createdPorts.get(0);

        // Browser destroys the port.
        port.destroy();

        // Verify the app's remote port received disconnect().
        Assert.assertTrue(fakeAppPort.isDisconnected);
    }

    // Test that if posting a message to the external app fails with a RemoteException
    // (e.g. app process died), the port immediately closes the channel with an error.
    @Test
    public void testPostMessageRemoteExceptionClosesChannel() throws Exception {
        FakeNativeMessagePort fakeAppPort = new FakeNativeMessagePort(null);
        Mockito.doAnswer(
                        invocation -> {
                            IConnectPortCallback callback = invocation.getArgument(1);
                            callback.onSuccess(fakeAppPort);
                            return null;
                        })
                .when(mMockExtensionService)
                .connectPort(Mockito.any(), Mockito.any());

        NativeMessageAndroidPort port = new NativeMessageAndroidPort();
        TestPortObserver portObserver = new TestPortObserver();
        port.setTestObserver(portObserver);

        Assert.assertNull(connectToApp(port));
        mTestContext.triggerServiceConnected(mFakeBrowserService.asBinder());
        RobolectricUtil.runAllBackgroundAndUi();

        // Configure fake app port to throw DeadObjectException when postMessage is called.
        fakeAppPort.shouldThrowOnPostMessage = true;

        var watcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(
                                "Extensions.NativeMessaging.Android.SentMessageSize.Success")
                        .expectNoRecords(
                                "Extensions.NativeMessaging.Android.SentMessageSize.TooLarge")
                        .build();

        // Sending a message fails and should trigger channel closure.
        port.forwardMessageToApp("msg_fail");
        RobolectricUtil.runAllBackgroundAndUi();

        Assert.assertEquals(
                "Error when communicating with the native messaging host.",
                portObserver.closedError);
        watcher.assertExpected();
    }

    // Test that if sending queued pending messages fails mid-flush (e.g. on message #2),
    // previous messages are delivered, the failure closes the channel, and remaining
    // pending messages are aborted.
    @Test
    public void testFlushPendingMessagesFails() throws Exception {
        FakeNativeMessagePort fakeAppPort = new FakeNativeMessagePort(null);
        fakeAppPort.failAfterMessageCount = 1;
        Mockito.doAnswer(
                        invocation -> {
                            IConnectPortCallback callback = invocation.getArgument(1);
                            callback.onSuccess(fakeAppPort);
                            return null;
                        })
                .when(mMockExtensionService)
                .connectPort(Mockito.any(), Mockito.any());

        NativeMessageAndroidPort port = new NativeMessageAndroidPort();
        TestPortObserver portObserver = new TestPortObserver();
        port.setTestObserver(portObserver);

        // Queue multiple messages while waiting for connection.
        port.forwardMessageToApp("pending_1");
        port.forwardMessageToApp("pending_2");
        port.forwardMessageToApp("pending_3");

        Assert.assertNull(connectToApp(port));
        mTestContext.triggerServiceConnected(mFakeBrowserService.asBinder());
        RobolectricUtil.runAllBackgroundAndUi();

        // Verify only message 1 was received, and message 3 was aborted after message 2 failed.
        Assert.assertEquals(List.of("pending_1"), fakeAppPort.receivedMessages);
        Assert.assertEquals(
                "Error when communicating with the native messaging host.",
                portObserver.closedError);
    }

    // Test that if the external app synchronously calls callback.onMessage during connectPort
    // (before returning the remote port), the message is delivered to the port without being
    // dropped.
    @Test
    public void testAppSendsMessageSynchronouslyDuringConnectPort() throws Exception {
        Mockito.doAnswer(
                        invocation -> {
                            IExtensionNativeMessageCallback messageReceiver =
                                    invocation.getArgument(0);
                            IConnectPortCallback callback = invocation.getArgument(1);
                            messageReceiver.onMessage(
                                    createPayload("synchronous_msg"), new Bundle());
                            callback.onSuccess(new FakeNativeMessagePort(messageReceiver));
                            return null;
                        })
                .when(mMockExtensionService)
                .connectPort(Mockito.any(), Mockito.any());

        NativeMessageAndroidPort port = new NativeMessageAndroidPort();
        TestPortObserver portObserver = new TestPortObserver();
        port.setTestObserver(portObserver);

        Assert.assertNull(connectToApp(port));
        mTestContext.triggerServiceConnected(mFakeBrowserService.asBinder());
        RobolectricUtil.runAllBackgroundAndUi();

        Assert.assertEquals(List.of("synchronous_msg"), portObserver.receivedMessages);
    }

    // Test that if an external app misbehaves and calls onSuccess multiple times or calls both
    // onSuccess and onError, only the first call is processed and subsequent remote ports
    // are immediately disconnected.
    @Test
    public void testConnectPortCallbackCalledMultipleTimes() throws Exception {
        FakeNativeMessagePort fakePort1 = new FakeNativeMessagePort(null);
        FakeNativeMessagePort fakePort2 = new FakeNativeMessagePort(null);

        Mockito.doAnswer(
                        invocation -> {
                            IConnectPortCallback callback = invocation.getArgument(1);
                            callback.onSuccess(fakePort1);
                            callback.onSuccess(fakePort2);
                            callback.onError("Conflicting error");
                            return null;
                        })
                .when(mMockExtensionService)
                .connectPort(Mockito.any(), Mockito.any());

        NativeMessageAndroidPort port = new NativeMessageAndroidPort();
        TestPortObserver portObserver = new TestPortObserver();
        port.setTestObserver(portObserver);

        Assert.assertNull(connectToApp(port));
        mTestContext.triggerServiceConnected(mFakeBrowserService.asBinder());
        RobolectricUtil.runAllBackgroundAndUi();

        // port1 should be connected and not disconnected.
        Assert.assertFalse(fakePort1.isDisconnected);
        // port2 should have been immediately disconnected.
        Assert.assertTrue(fakePort2.isDisconnected);
        // Port observer should not have received any closed channel error.
        Assert.assertNull(portObserver.closedError);
    }

    // Test that when multiple ports are added with staggered timing:
    // Port 1 is added -> half timeout elapses -> Port 2 is added -> Port 1 times out ->
    // Port 2 connection succeeds afterwards and functions properly.
    @Test
    public void testPort1TimesOutBeforePort2Connects() throws Exception {
        List<IConnectPortCallback> callbacks = new ArrayList<>();
        List<FakeNativeMessagePort> fakePorts = new ArrayList<>();
        Mockito.doAnswer(
                        invocation -> {
                            IExtensionNativeMessageCallback msgCb = invocation.getArgument(0);
                            IConnectPortCallback connectPortCb = invocation.getArgument(1);
                            callbacks.add(connectPortCb);
                            fakePorts.add(new FakeNativeMessagePort(msgCb));
                            return null;
                        })
                .when(mMockExtensionService)
                .connectPort(Mockito.any(), Mockito.any());

        // 1. Add port 1 and trigger connection.
        NativeMessageAndroidPort port1 = new NativeMessageAndroidPort();
        TestPortObserver observer1 = new TestPortObserver();
        port1.setTestObserver(observer1);
        Assert.assertNull(connectToApp(port1));
        mTestContext.triggerServiceConnected(mFakeBrowserService.asBinder());
        RobolectricUtil.runAllBackgroundAndUi();
        Assert.assertEquals(1, callbacks.size());

        // 2. Let half the timeout pass (15s).
        ShadowLooper.idleMainLooper(
                NativeMessagingConnection.CONNECT_TIMEOUT_MS / 2, TimeUnit.MILLISECONDS);

        // 3. Add port 2.
        NativeMessageAndroidPort port2 = new NativeMessageAndroidPort();
        TestPortObserver observer2 = new TestPortObserver();
        port2.setTestObserver(observer2);
        Assert.assertNull(connectToApp(port2));
        RobolectricUtil.runAllBackgroundAndUi();
        Assert.assertEquals(2, callbacks.size());

        // 4. Advance another half timeout (total 30s for port 1, 15s for port 2). Port 1 times out.
        ShadowLooper.idleMainLooper(
                NativeMessagingConnection.CONNECT_TIMEOUT_MS / 2, TimeUnit.MILLISECONDS);
        Assert.assertEquals(
                "Could not connect port to " + TARGET_PACKAGE + ".", observer1.closedError);
        Assert.assertNull(observer2.closedError);

        // 5. Mock app returns success for port 2.
        callbacks.get(1).onSuccess(fakePorts.get(1));
        RobolectricUtil.runAllBackgroundAndUi();

        Assert.assertNull(observer2.closedError);
        port2.forwardMessageToApp("msg_from_port_2");
        RobolectricUtil.runAllBackgroundAndUi();
        Assert.assertEquals(List.of("msg_from_port_2"), fakePorts.get(1).receivedMessages);
    }

    // Test that when multiple ports are added with staggered timing:
    // Port 1 is added -> half timeout elapses -> Port 2 is added -> Port 2 connection succeeds ->
    // Port 1 times out -> Port 2 survives and remains functional.
    @Test
    public void testPort2ConnectsBeforePort1TimesOut() throws Exception {
        List<IConnectPortCallback> callbacks = new ArrayList<>();
        List<FakeNativeMessagePort> fakePorts = new ArrayList<>();
        Mockito.doAnswer(
                        invocation -> {
                            IExtensionNativeMessageCallback msgCb = invocation.getArgument(0);
                            IConnectPortCallback connectPortCb = invocation.getArgument(1);
                            callbacks.add(connectPortCb);
                            fakePorts.add(new FakeNativeMessagePort(msgCb));
                            return null;
                        })
                .when(mMockExtensionService)
                .connectPort(Mockito.any(), Mockito.any());

        // 1. Add port 1 and trigger connection.
        NativeMessageAndroidPort port1 = new NativeMessageAndroidPort();
        TestPortObserver observer1 = new TestPortObserver();
        port1.setTestObserver(observer1);
        Assert.assertNull(connectToApp(port1));
        mTestContext.triggerServiceConnected(mFakeBrowserService.asBinder());
        RobolectricUtil.runAllBackgroundAndUi();
        Assert.assertEquals(1, callbacks.size());

        // 2. Let half the timeout pass (15s).
        ShadowLooper.idleMainLooper(
                NativeMessagingConnection.CONNECT_TIMEOUT_MS / 2, TimeUnit.MILLISECONDS);

        // 3. Add port 2.
        NativeMessageAndroidPort port2 = new NativeMessageAndroidPort();
        TestPortObserver observer2 = new TestPortObserver();
        port2.setTestObserver(observer2);
        Assert.assertNull(connectToApp(port2));
        RobolectricUtil.runAllBackgroundAndUi();
        Assert.assertEquals(2, callbacks.size());

        // 4. Port 2 connection succeeds.
        callbacks.get(1).onSuccess(fakePorts.get(1));
        RobolectricUtil.runAllBackgroundAndUi();
        Assert.assertNull(observer2.closedError);

        // 5. Advance another half timeout. Port 1 times out; Port 2 survives.
        ShadowLooper.idleMainLooper(
                NativeMessagingConnection.CONNECT_TIMEOUT_MS / 2, TimeUnit.MILLISECONDS);
        Assert.assertEquals(
                "Could not connect port to " + TARGET_PACKAGE + ".", observer1.closedError);
        Assert.assertNull(observer2.closedError);
        Assert.assertFalse(fakePorts.get(1).isDisconnected);

        port2.forwardMessageToApp("port_2_survives");
        RobolectricUtil.runAllBackgroundAndUi();
        Assert.assertEquals(List.of("port_2_survives"), fakePorts.get(1).receivedMessages);
    }

    // Test that if an app delivers a late onSuccess after connectPort has timed out,
    // the late remote port is immediately disconnected and the closed port is not modified.
    @Test
    public void testLateOnSuccessAfterTimeoutDisconnectsRemotePort() throws Exception {
        IConnectPortCallback[] capturedCallback = new IConnectPortCallback[1];
        Mockito.doAnswer(
                        invocation -> {
                            capturedCallback[0] = invocation.getArgument(1);
                            return null;
                        })
                .when(mMockExtensionService)
                .connectPort(Mockito.any(), Mockito.any());

        NativeMessageAndroidPort port = new NativeMessageAndroidPort();
        TestPortObserver observer = new TestPortObserver();
        port.setTestObserver(observer);

        Assert.assertNull(connectToApp(port));
        mTestContext.triggerServiceConnected(mFakeBrowserService.asBinder());
        RobolectricUtil.runAllBackgroundAndUi();
        Assert.assertNotNull(capturedCallback[0]);

        // Trigger timeout.
        ShadowLooper.idleMainLooper(
                NativeMessagingConnection.CONNECT_TIMEOUT_MS, TimeUnit.MILLISECONDS);
        Assert.assertEquals(
                "Could not connect port to " + TARGET_PACKAGE + ".", observer.closedError);

        // App responds late with onSuccess.
        FakeNativeMessagePort latePort = new FakeNativeMessagePort(null);
        capturedCallback[0].onSuccess(latePort);
        RobolectricUtil.runAllBackgroundAndUi();

        // The late remote port should be immediately disconnected.
        Assert.assertTrue(latePort.isDisconnected);
        // Closed error is unchanged.
        Assert.assertEquals(
                "Could not connect port to " + TARGET_PACKAGE + ".", observer.closedError);
    }

    // Test that if an app delivers a late onError after connectPort has timed out,
    // it is ignored and does not overwrite the timeout error or call the observer again.
    @Test
    public void testLateOnErrorAfterTimeoutIsIgnored() throws Exception {
        IConnectPortCallback[] capturedCallback = new IConnectPortCallback[1];
        Mockito.doAnswer(
                        invocation -> {
                            capturedCallback[0] = invocation.getArgument(1);
                            return null;
                        })
                .when(mMockExtensionService)
                .connectPort(Mockito.any(), Mockito.any());

        NativeMessageAndroidPort port = new NativeMessageAndroidPort();
        int[] closeCallCount = new int[1];
        port.setTestObserver(
                new NativeMessageAndroidPort.TestObserver() {
                    @Override
                    public void onMessageFromApp(String message) {}

                    @Override
                    public void onChannelClosed(String errorMessage) {
                        closeCallCount[0]++;
                    }
                });

        Assert.assertNull(connectToApp(port));
        mTestContext.triggerServiceConnected(mFakeBrowserService.asBinder());
        RobolectricUtil.runAllBackgroundAndUi();
        Assert.assertNotNull(capturedCallback[0]);

        // Trigger timeout.
        ShadowLooper.idleMainLooper(
                NativeMessagingConnection.CONNECT_TIMEOUT_MS, TimeUnit.MILLISECONDS);
        Assert.assertEquals(1, closeCallCount[0]);

        // App responds late with onError.
        capturedCallback[0].onError("late error");
        RobolectricUtil.runAllBackgroundAndUi();

        // Observer should not be notified a second time.
        Assert.assertEquals(1, closeCallCount[0]);
    }

    // Test that hitting TransactionTooLargeException records SentMessageSize.TooLarge and closes
    // channel.
    @Test
    public void testSendMessageTransactionTooLargeHistogram() throws RemoteException {
        FakeNativeMessagePort fakeAppPort = new FakeNativeMessagePort(null);
        Mockito.doAnswer(
                        invocation -> {
                            IConnectPortCallback callback = invocation.getArgument(1);
                            callback.onSuccess(fakeAppPort);
                            return null;
                        })
                .when(mMockExtensionService)
                .connectPort(Mockito.any(), Mockito.any());

        NativeMessageAndroidPort port = new NativeMessageAndroidPort();
        TestPortObserver portObserver = new TestPortObserver();
        port.setTestObserver(portObserver);

        Assert.assertNull(connectToApp(port));
        mTestContext.triggerServiceConnected(mFakeBrowserService.asBinder());
        RobolectricUtil.runAllBackgroundAndUi();

        fakeAppPort.shouldThrowTransactionTooLarge = true;

        String message = "msg_large";
        int expectedSize = message.getBytes(StandardCharsets.UTF_8).length;

        var tooLargeWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Extensions.NativeMessaging.Android.SentMessageSize.TooLarge",
                        expectedSize);
        var successWatcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(
                                "Extensions.NativeMessaging.Android.SentMessageSize.Success")
                        .build();

        port.forwardMessageToApp(message);
        RobolectricUtil.runAllBackgroundAndUi();

        tooLargeWatcher.assertExpected();
        successWatcher.assertExpected();
        Assert.assertEquals(
                "Error when communicating with the native messaging host.",
                portObserver.closedError);
    }
}
